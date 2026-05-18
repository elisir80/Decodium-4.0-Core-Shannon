// 1.0.238 (Phase 5.2 perf roadmap):
// DecodeHistoryWorker.cpp -- vedi header per design overview.
//
// Mapping QVariantMap (entry come prodotta da enrichDecodeEntry +
// appendDecodeMapToList) -> schema columns:
//
//   entry["time"]       (QString "HHMMSS" UTC token) | "timestamp" (qint64)
//                       --> ts_utc INTEGER (ms epoch)
//   entry["band"]       --> band TEXT
//   entry["freq"]       (QString MHz "14074.123")     --> freq_hz INTEGER
//   entry["mode"]       --> mode TEXT
//   entry["submode"]    --> submode TEXT (NULL se vuoto)
//   entry["dxCallsign"] --> callsign_dx TEXT
//   entry["fromCall"]   --> callsign_de TEXT
//   entry["grid"]       --> grid TEXT
//   entry["db"]         (QString "-12" o "TX")        --> snr_db INTEGER
//   entry["dt"]         (QString "0.5")               --> dt_s REAL
//   entry["df"]         (QString)                     --> df_hz INTEGER
//   entry["message"]    --> message TEXT NOT NULL
//   entry["quality"]    --> confidence INTEGER
//   m_sessionId         --> session_id INTEGER NOT NULL

#include "DecodeHistoryWorker.h"

#include <QDateTime>
#include <QSqlError>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <QtGlobal>

namespace {

// "HHMMSS" -> ms epoch (oggi, UTC). Per il giorno corrente assumiamo UTC odierno.
// Per gli edge case (decode arrivata appena dopo mezzanotte ma timestamp HHMMSS
// del giorno precedente) tolleriamo fino a 12h all'indietro.
qint64 parseTimestampMs(QVariantMap const& entry)
{
    QVariant const tsVar = entry.value(QStringLiteral("timestamp"));
    if (tsVar.isValid() && tsVar.canConvert<qint64>()) {
        qint64 const ts = tsVar.toLongLong();
        if (ts > 0) return ts;
    }
    QString const tok = entry.value(QStringLiteral("time")).toString().trimmed();
    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    if (tok.size() == 6) {
        bool ok1 = false, ok2 = false, ok3 = false;
        int const h = tok.left(2).toInt(&ok1);
        int const m = tok.mid(2, 2).toInt(&ok2);
        int const s = tok.mid(4, 2).toInt(&ok3);
        if (ok1 && ok2 && ok3) {
            QDateTime cand(nowUtc.date(), QTime(h, m, s), Qt::UTC);
            qint64 const diffMs = nowUtc.toMSecsSinceEpoch() - cand.toMSecsSinceEpoch();
            if (diffMs < -3600 * 1000) {  // entry "futura" > 1h: probabilmente ieri
                cand = cand.addDays(-1);
            }
            return cand.toMSecsSinceEpoch();
        }
    }
    return nowUtc.toMSecsSinceEpoch();
}

qint64 parseFreqHz(QVariantMap const& entry)
{
    QString s = entry.value(QStringLiteral("freq")).toString().trimmed();
    if (s.isEmpty()) return 0;
    // freq e' tipicamente in MHz come "14074.123" o "14.074123" a seconda
    // del contesto. Heuristica: se contiene un punto e parte intera <= 4
    // cifre, assumiamo MHz, altrimenti kHz/Hz literal.
    bool ok = false;
    double const v = s.toDouble(&ok);
    if (!ok) return 0;
    // Tipico FT8: "14074.123" -> 14.074123 MHz -> 14074123 Hz. Treat as kHz
    // when integer part > ~999 (e.g. 14074 looks like kHz already).
    // Treat as MHz when integer part <= ~999 (e.g. 14.074).
    int const dotIdx = s.indexOf(QLatin1Char('.'));
    int const intDigits = (dotIdx >= 0) ? dotIdx : s.size();
    if (intDigits >= 4) {
        // "14074.123" -> kHz
        return static_cast<qint64>(v * 1000.0 + 0.5);
    }
    // "14.074123" -> MHz
    return static_cast<qint64>(v * 1000000.0 + 0.5);
}

QVariant parseSnr(QVariantMap const& entry)
{
    QString const s = entry.value(QStringLiteral("db")).toString().trimmed();
    if (s.isEmpty()) return QVariant(QMetaType(QMetaType::Int));
    // "TX" non e' un SNR misurato. Lascia NULL.
    if (s.compare(QStringLiteral("TX"), Qt::CaseInsensitive) == 0) {
        return QVariant(QMetaType(QMetaType::Int));
    }
    bool ok = false;
    int const v = s.toInt(&ok);
    return ok ? QVariant(v) : QVariant(QMetaType(QMetaType::Int));
}

QVariant parseDt(QVariantMap const& entry)
{
    QString const s = entry.value(QStringLiteral("dt")).toString().trimmed();
    if (s.isEmpty()) return QVariant(QMetaType(QMetaType::Double));
    bool ok = false;
    double const v = s.toDouble(&ok);
    return ok ? QVariant(v) : QVariant(QMetaType(QMetaType::Double));
}

QVariant parseDfHz(QVariantMap const& entry)
{
    QString const s = entry.value(QStringLiteral("df")).toString().trimmed();
    if (s.isEmpty()) return QVariant(QMetaType(QMetaType::LongLong));
    bool ok = false;
    qint64 const v = s.toLongLong(&ok);
    return ok ? QVariant(v) : QVariant(QMetaType(QMetaType::LongLong));
}

QVariant parseConfidence(QVariantMap const& entry)
{
    QVariant const q = entry.value(QStringLiteral("quality"));
    if (!q.isValid()) return QVariant(QMetaType(QMetaType::Int));
    bool ok = false;
    int const v = q.toInt(&ok);
    return ok ? QVariant(v) : QVariant(QMetaType(QMetaType::Int));
}

QVariant strOrNull(QString const& s)
{
    return s.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(s);
}

} // namespace

DecodeHistoryWorker::DecodeHistoryWorker(QString dbPath,
                                         qint64 sessionId,
                                         QObject* parent)
    : QObject(parent)
    , m_dbPath(std::move(dbPath))
    , m_sessionId(sessionId)
{
    m_buffer.reserve(kMaxBuffer);
}

DecodeHistoryWorker::~DecodeHistoryWorker()
{
    // Nota: shutdown() dovrebbe essere chiamato dal bridge PRIMA del
    // distruttore, in modo che la connessione SQLite venga chiusa sul
    // thread worker. Se siamo qui senza shutdown(), tentiamo un best-effort
    // sul thread corrente (potrebbe essere il main thread durante teardown
    // del QThread parent).
    if (m_initialized) {
        if (m_db.isValid() && m_db.isOpen()) {
            m_db.close();
        }
        m_db = QSqlDatabase();   // detach prima del removeDatabase
        QSqlDatabase::removeDatabase(m_connName);
        m_initialized = false;
    }
}

void DecodeHistoryWorker::initialize()
{
    if (m_initialized) return;

    if (!QSqlDatabase::drivers().contains(QStringLiteral("QSQLITE"))) {
        qWarning("[DecodeHistoryWorker] QSQLITE driver missing");
        return;
    }

    // Connessione named, separata dalla defaultConnection del main thread.
    if (QSqlDatabase::contains(m_connName)) {
        // Connessione precedente non chiusa correttamente: forziamo cleanup.
        QSqlDatabase::removeDatabase(m_connName);
    }
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connName);
    m_db.setDatabaseName(m_dbPath);
    if (!m_db.open()) {
        qWarning("[DecodeHistoryWorker] DB open failed: %s",
                 m_db.lastError().text().toLocal8Bit().constData());
        return;
    }

    // PRAGMA per allinearci alla connessione del main: WAL e' modalita' di
    // file, non per-connection, ma synchronous/busy_timeout/temp_store si'.
    auto applyPragma = [this](char const* sql) {
        QSqlQuery q(m_db);
        if (!q.exec(QString::fromLatin1(sql))) {
            qWarning("[DecodeHistoryWorker] pragma failed: %s | %s",
                     sql,
                     q.lastError().text().toLocal8Bit().constData());
        }
    };
    applyPragma("PRAGMA synchronous=NORMAL");
    applyPragma("PRAGMA busy_timeout=5000");
    applyPragma("PRAGMA temp_store=MEMORY");

    m_insertStmt = QSqlQuery(m_db);
    if (!m_insertStmt.prepare(QStringLiteral(
            "INSERT INTO decodes ("
            " ts_utc, band, freq_hz, mode, submode,"
            " callsign_dx, callsign_de, grid, snr_db, dt_s, df_hz,"
            " message, confidence, session_id"
            ") VALUES ("
            " :ts_utc, :band, :freq_hz, :mode, :submode,"
            " :callsign_dx, :callsign_de, :grid, :snr_db, :dt_s, :df_hz,"
            " :message, :confidence, :session_id"
            ")"))) {
        qWarning("[DecodeHistoryWorker] prepare INSERT failed: %s",
                 m_insertStmt.lastError().text().toLocal8Bit().constData());
        m_db.close();
        return;
    }

    // Timer di flush (vive sul thread worker dopo moveToThread del worker:
    // questo metodo va invocato sul thread worker).
    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(kFlushMs);
    m_flushTimer->setSingleShot(false);
    connect(m_flushTimer, &QTimer::timeout,
            this, &DecodeHistoryWorker::flushBuffer);
    m_flushTimer->start();

    m_initialized = true;
    qInfo("[DecodeHistoryWorker] initialized: session=%lld db=%s",
          static_cast<long long>(m_sessionId),
          m_dbPath.toLocal8Bit().constData());
}

void DecodeHistoryWorker::shutdown()
{
    if (m_flushTimer) {
        m_flushTimer->stop();
    }
    // Flush finale per non perdere entry pendenti.
    if (m_initialized && !m_buffer.isEmpty()) {
        flushBuffer();
    }
    if (m_initialized) {
        m_insertStmt.clear();
        if (m_db.isValid() && m_db.isOpen()) {
            m_db.close();
        }
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connName);
        m_initialized = false;
    }
}

void DecodeHistoryWorker::enqueueDecode(QVariantMap entry)
{
    if (m_buffer.size() >= kMaxBuffer) {
        // Backpressure FIFO: drop la entry piu' vecchia.
        m_buffer.removeFirst();
        ++m_dropCount;
        if (m_dropCount - m_lastDropWarnAt >= 100) {
            qWarning("[DecodeHistoryWorker] backpressure: %d total drops",
                     m_dropCount);
            m_lastDropWarnAt = m_dropCount;
        }
    }
    m_buffer.append(std::move(entry));
}

void DecodeHistoryWorker::flushBuffer()
{
    if (!m_initialized) return;
    if (m_buffer.isEmpty()) return;

    QVector<QVariantMap> batch;
    batch.swap(m_buffer);

    if (!m_db.transaction()) {
        // Senza transaction esplicita ogni INSERT diventa un commit:
        // performance pessima ma corretta. Logghiamo e proviamo lo stesso.
        qWarning("[DecodeHistoryWorker] transaction begin failed: %s",
                 m_db.lastError().text().toLocal8Bit().constData());
    }

    int inserted = 0;
    for (QVariantMap const& e : batch) {
        m_insertStmt.bindValue(QStringLiteral(":ts_utc"),
                               parseTimestampMs(e));
        m_insertStmt.bindValue(QStringLiteral(":band"),
                               strOrNull(e.value(QStringLiteral("band")).toString()));
        m_insertStmt.bindValue(QStringLiteral(":freq_hz"),
                               parseFreqHz(e));
        m_insertStmt.bindValue(QStringLiteral(":mode"),
                               e.value(QStringLiteral("mode")).toString());
        m_insertStmt.bindValue(QStringLiteral(":submode"),
                               strOrNull(e.value(QStringLiteral("submode")).toString()));
        m_insertStmt.bindValue(QStringLiteral(":callsign_dx"),
                               strOrNull(e.value(QStringLiteral("dxCallsign")).toString()));
        m_insertStmt.bindValue(QStringLiteral(":callsign_de"),
                               strOrNull(e.value(QStringLiteral("fromCall")).toString()));
        m_insertStmt.bindValue(QStringLiteral(":grid"),
                               strOrNull(e.value(QStringLiteral("grid")).toString()));
        m_insertStmt.bindValue(QStringLiteral(":snr_db"),
                               parseSnr(e));
        m_insertStmt.bindValue(QStringLiteral(":dt_s"),
                               parseDt(e));
        m_insertStmt.bindValue(QStringLiteral(":df_hz"),
                               parseDfHz(e));
        m_insertStmt.bindValue(QStringLiteral(":message"),
                               e.value(QStringLiteral("message")).toString());
        m_insertStmt.bindValue(QStringLiteral(":confidence"),
                               parseConfidence(e));
        m_insertStmt.bindValue(QStringLiteral(":session_id"),
                               m_sessionId);
        if (m_insertStmt.exec()) {
            ++inserted;
        } else {
            qWarning("[DecodeHistoryWorker] INSERT failed: %s",
                     m_insertStmt.lastError().text().toLocal8Bit().constData());
        }
    }

    if (!m_db.commit()) {
        qWarning("[DecodeHistoryWorker] commit failed: %s",
                 m_db.lastError().text().toLocal8Bit().constData());
        m_db.rollback();
    }

    if (inserted > 0) {
        emit decodesPersisted(inserted, m_sessionId);
    }
}
