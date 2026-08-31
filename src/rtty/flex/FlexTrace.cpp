#include "flex/FlexTrace.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

namespace decortty::flex {

FlexTrace& FlexTrace::instance()
{
    static FlexTrace trace;
    return trace;
}

FlexTrace::FlexTrace()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    m_path = dir + QStringLiteral("/decortty-flex.log");

    // Si tronca a ogni avvio. Il registro serve alla sessione che comincia
    // adesso, e un file che cresce fra una prova e l'altra costringe a cercare
    // dove finisce quella di ieri.
    m_file.setFileName(m_path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;

    // Il nome con la versione, quando c'e'. Uno strumento di prova non la
    // imposta, e "DecoRTTY  —" con due spazi in mezzo, in testa al file che
    // serve a capire cosa non andava, sembra un pezzo mancante.
    const QString version = QCoreApplication::applicationVersion();
    m_out.setDevice(&m_file);
    m_out << QStringLiteral("# DecoRTTY%1 — registro del dialogo con la radio\n")
                 .arg(version.isEmpty() ? QString() : QLatin1Char(' ') + version)
          << QStringLiteral("# %1\n")
                 .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
          << QStringLiteral("#   >  comando mandato alla radio\n")
          << QStringLiteral("#   <  riga arrivata dalla radio\n")
          << QStringLiteral("#   .  decisione presa da DecoRTTY\n\n");
    m_out.flush();
}

FlexTrace::~FlexTrace()
{
    if (m_file.isOpen()) {
        m_out.flush();
        m_file.close();
    }
}

void FlexTrace::write(QChar mark, const QString& text)
{
    if (!m_file.isOpen() || m_full)
        return;

    if (m_written > kMaxBytes) {
        m_full = true;
        m_out << QStringLiteral("\n# il registro ha raggiunto il tetto e si ferma qui\n");
        m_out.flush();
        return;
    }

    const QString line = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"))
                       + QLatin1Char(' ') + mark + QLatin1Char(' ') + text + QLatin1Char('\n');
    m_out << line;
    // Si versa a ogni riga. Un programma che se ne va di traverso porterebbe via
    // il buffer, cioe' proprio le ultime righe — le uniche che spiegano perche'
    // se n'e' andato.
    m_out.flush();
    m_written += line.size();
}

void FlexTrace::sent(const QString& command)      { write(QLatin1Char('>'), command); }
void FlexTrace::received(const QString& line)     { write(QLatin1Char('<'), line); }
void FlexTrace::note(const QString& text)         { write(QLatin1Char('.'), text); }

} // namespace decortty::flex
