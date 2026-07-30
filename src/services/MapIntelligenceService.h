#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QThreadPool>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>

class MapLayerModel;
class MapBaseMapService;
class MapExternalOverlayService;
class MapOperationsService;
class MapPskFeedService;
class QTimer;

class MapIntelligenceService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* layerModel READ layerModel CONSTANT)
    Q_PROPERTY(QObject* baseMapService READ baseMapService CONSTANT)
    Q_PROPERTY(QObject* externalOverlayService READ externalOverlayService CONSTANT)
    Q_PROPERTY(QObject* operationsService READ operationsService CONSTANT)
    Q_PROPERTY(QObject* pskFeedService READ pskFeedService CONSTANT)
    Q_PROPERTY(QVariantList coverageCells READ coverageCells NOTIFY coverageChanged)
    Q_PROPERTY(QVariantList roster READ roster NOTIFY rosterChanged)
    Q_PROPERTY(QVariantList rosterPreferences READ rosterPreferences NOTIFY rosterPreferencesChanged)
    Q_PROPERTY(QVariantList awards READ awards NOTIFY awardsChanged)
    Q_PROPERTY(QVariantList alerts READ alerts NOTIFY alertsChanged)
    Q_PROPERTY(QVariantList spotHeatmap READ spotHeatmap NOTIFY spotAnalyticsChanged)
    Q_PROPERTY(QVariantList spotTimeline READ spotTimeline NOTIFY spotAnalyticsChanged)
    Q_PROPERTY(QVariantList spotPaths READ spotPaths NOTIFY spotAnalyticsChanged)
    Q_PROPERTY(QVariantList bandActivity READ bandActivity NOTIFY bandActivityChanged)
    Q_PROPERTY(QVariantList bandActivityTimeline READ bandActivityTimeline NOTIFY bandActivityChanged)
    Q_PROPERTY(QVariantMap bandActivitySummary READ bandActivitySummary NOTIFY bandActivityChanged)
    Q_PROPERTY(int bandActivityWindowHours READ bandActivityWindowHours WRITE setBandActivityWindowHours NOTIFY bandActivityWindowHoursChanged)
    Q_PROPERTY(QVariantList rosterRules READ rosterRules NOTIFY rosterRulesChanged)
    Q_PROPERTY(QVariantMap statistics READ statistics NOTIFY statisticsChanged)
    Q_PROPERTY(QString selectedGrid READ selectedGrid NOTIFY gridDetailsChanged)
    Q_PROPERTY(QVariantMap selectedGridSummary READ selectedGridSummary NOTIFY gridDetailsChanged)
    Q_PROPERTY(QVariantList selectedGridLive READ selectedGridLive NOTIFY gridDetailsChanged)
    Q_PROPERTY(QVariantList selectedGridQsos READ selectedGridQsos NOTIFY gridDetailsChanged)
    Q_PROPERTY(bool gridDetailsLoading READ gridDetailsLoading NOTIFY gridDetailsLoadingChanged)
    Q_PROPERTY(QStringList availableBands READ availableBands NOTIFY filtersChanged)
    Q_PROPERTY(QStringList availableModes READ availableModes NOTIFY filtersChanged)
    Q_PROPERTY(QStringList availablePeriods READ availablePeriods CONSTANT)
    Q_PROPERTY(QStringList availableContinents READ availableContinents NOTIFY filtersChanged)
    Q_PROPERTY(QStringList availableDxcc READ availableDxcc NOTIFY filtersChanged)
    Q_PROPERTY(QStringList availableSources READ availableSources NOTIFY filtersChanged)
    Q_PROPERTY(QStringList availableRosterStatuses READ availableRosterStatuses CONSTANT)
    Q_PROPERTY(QStringList availableRosterHuntScopes READ availableRosterHuntScopes CONSTANT)
    Q_PROPERTY(QStringList availableAwardPrograms READ availableAwardPrograms CONSTANT)
    Q_PROPERTY(QStringList availableAwardGoals READ availableAwardGoals CONSTANT)
    Q_PROPERTY(QStringList availablePskDisplayModes READ availablePskDisplayModes CONSTANT)
    Q_PROPERTY(QStringList availableSpotAgeFilters READ availableSpotAgeFilters CONSTANT)
    Q_PROPERTY(QStringList availableCorrelationFilters READ availableCorrelationFilters CONSTANT)
    Q_PROPERTY(QStringList availableRosterColumns READ availableRosterColumns CONSTANT)
    Q_PROPERTY(QStringList availableCallLookupProviders READ availableCallLookupProviders CONSTANT)
    Q_PROPERTY(QString bandFilter READ bandFilter WRITE setBandFilter NOTIFY bandFilterChanged)
    Q_PROPERTY(QString modeFilter READ modeFilter WRITE setModeFilter NOTIFY modeFilterChanged)
    Q_PROPERTY(QString periodFilter READ periodFilter WRITE setPeriodFilter NOTIFY periodFilterChanged)
    Q_PROPERTY(QString continentFilter READ continentFilter WRITE setContinentFilter NOTIFY continentFilterChanged)
    Q_PROPERTY(QString dxccFilter READ dxccFilter WRITE setDxccFilter NOTIFY dxccFilterChanged)
    Q_PROPERTY(QString sourceFilter READ sourceFilter WRITE setSourceFilter NOTIFY sourceFilterChanged)
    Q_PROPERTY(bool cqOnly READ cqOnly WRITE setCqOnly NOTIFY cqOnlyChanged)
    Q_PROPERTY(QString rosterSort READ rosterSort WRITE setRosterSort NOTIFY rosterSortChanged)
    Q_PROPERTY(bool rosterSortDescending READ rosterSortDescending WRITE setRosterSortDescending NOTIFY rosterSortDescendingChanged)
    Q_PROPERTY(QString rosterStatusFilter READ rosterStatusFilter WRITE setRosterStatusFilter NOTIFY rosterStatusFilterChanged)
    Q_PROPERTY(QString rosterHuntScope READ rosterHuntScope WRITE setRosterHuntScope NOTIFY rosterHuntScopeChanged)
    Q_PROPERTY(int rosterRetentionMinutes READ rosterRetentionMinutes WRITE setRosterRetentionMinutes NOTIFY rosterRetentionMinutesChanged)
    Q_PROPERTY(bool rosterCqOnly READ rosterCqOnly WRITE setRosterCqOnly NOTIFY rosterCqOnlyChanged)
    Q_PROPERTY(QString rosterTextFilter READ rosterTextFilter WRITE setRosterTextFilter NOTIFY rosterTextFilterChanged)
    Q_PROPERTY(QString rosterTextMode READ rosterTextMode WRITE setRosterTextMode NOTIFY rosterTextModeChanged)
    Q_PROPERTY(QString activeAwardProgram READ activeAwardProgram WRITE setActiveAwardProgram NOTIFY activeAwardProgramChanged)
    Q_PROPERTY(QString awardGoal READ awardGoal WRITE setAwardGoal NOTIFY awardGoalChanged)
    Q_PROPERTY(int gridPrecision READ gridPrecision WRITE setGridPrecision NOTIFY gridPrecisionChanged)
    Q_PROPERTY(int liveDecayMinutes READ liveDecayMinutes WRITE setLiveDecayMinutes NOTIFY liveDecayMinutesChanged)
    Q_PROPERTY(bool splitGridEnabled READ splitGridEnabled WRITE setSplitGridEnabled NOTIFY splitGridEnabledChanged)
    Q_PROPERTY(bool coveragePushPinsEnabled READ coveragePushPinsEnabled WRITE setCoveragePushPinsEnabled NOTIFY coveragePushPinsEnabledChanged)
    Q_PROPERTY(bool timeZoneOverlayEnabled READ timeZoneOverlayEnabled WRITE setTimeZoneOverlayEnabled NOTIFY timeZoneOverlayEnabledChanged)
    Q_PROPERTY(QString pskDisplayMode READ pskDisplayMode WRITE setPskDisplayMode NOTIFY pskDisplayModeChanged)
    Q_PROPERTY(int pskOpacityPercent READ pskOpacityPercent WRITE setPskOpacityPercent NOTIFY pskOpacityPercentChanged)
    Q_PROPERTY(QString spotAgeFilter READ spotAgeFilter WRITE setSpotAgeFilter NOTIFY spotAgeFilterChanged)
    Q_PROPERTY(QString spotCorrelationFilter READ spotCorrelationFilter WRITE setSpotCorrelationFilter NOTIFY spotCorrelationFilterChanged)
    Q_PROPERTY(QStringList rosterVisibleColumns READ rosterVisibleColumns WRITE setRosterVisibleColumns NOTIFY rosterVisibleColumnsChanged)
    Q_PROPERTY(QString callLookupProvider READ callLookupProvider WRITE setCallLookupProvider NOTIFY callLookupProviderChanged)
    Q_PROPERTY(bool alertNewGridEnabled READ alertNewGridEnabled WRITE setAlertNewGridEnabled NOTIFY alertRulesChanged)
    Q_PROPERTY(bool alertNewDxccEnabled READ alertNewDxccEnabled WRITE setAlertNewDxccEnabled NOTIFY alertRulesChanged)
    Q_PROPERTY(bool alertCqEnabled READ alertCqEnabled WRITE setAlertCqEnabled NOTIFY alertRulesChanged)
    Q_PROPERTY(QString alertCallPattern READ alertCallPattern WRITE setAlertCallPattern NOTIFY alertRulesChanged)
    Q_PROPERTY(bool workedLayerEnabled READ workedLayerEnabled WRITE setWorkedLayerEnabled NOTIFY workedLayerEnabledChanged)
    Q_PROPERTY(bool confirmedLayerEnabled READ confirmedLayerEnabled WRITE setConfirmedLayerEnabled NOTIFY confirmedLayerEnabledChanged)
    Q_PROPERTY(bool liveLayerEnabled READ liveLayerEnabled WRITE setLiveLayerEnabled NOTIFY liveLayerEnabledChanged)
    Q_PROPERTY(bool activeLayerEnabled READ activeLayerEnabled WRITE setActiveLayerEnabled NOTIFY activeLayerEnabledChanged)
    Q_PROPERTY(bool missingLayerEnabled READ missingLayerEnabled WRITE setMissingLayerEnabled NOTIFY missingLayerEnabledChanged)
    Q_PROPERTY(bool pskLayerEnabled READ pskLayerEnabled WRITE setPskLayerEnabled NOTIFY pskLayerEnabledChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY sourcePathChanged)
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)
    Q_PROPERTY(int qsoCount READ qsoCount NOTIFY coverageChanged)
    Q_PROPERTY(int workedGridCount READ workedGridCount NOTIFY coverageChanged)
    Q_PROPERTY(int confirmedGridCount READ confirmedGridCount NOTIFY coverageChanged)
    Q_PROPERTY(int activeGridCount READ activeGridCount NOTIFY coverageChanged)
    Q_PROPERTY(int missingGridCount READ missingGridCount NOTIFY coverageChanged)
    Q_PROPERTY(int liveSpotCount READ liveSpotCount NOTIFY rosterChanged)
    Q_PROPERTY(int rosterCount READ rosterCount NOTIFY rosterChanged)
    Q_PROPERTY(int rosterWantedCount READ rosterWantedCount NOTIFY rosterChanged)
    Q_PROPERTY(int rosterNewCount READ rosterNewCount NOTIFY rosterChanged)
    Q_PROPERTY(int rosterUnconfirmedCount READ rosterUnconfirmedCount NOTIFY rosterChanged)
    Q_PROPERTY(int rosterPreferenceCount READ rosterPreferenceCount NOTIFY rosterPreferencesChanged)
    Q_PROPERTY(int unreadAlertCount READ unreadAlertCount NOTIFY alertsChanged)

public:
    explicit MapIntelligenceService(QObject* parent = nullptr,
                                    const QString& databasePath = {});
    ~MapIntelligenceService() override;

    QObject* layerModel() const;
    QObject* baseMapService() const;
    QObject* externalOverlayService() const;
    QObject* operationsService() const;
    QObject* pskFeedService() const;
    QVariantList coverageCells() const { return m_coverageCells; }
    QVariantList roster() const { return m_roster; }
    QVariantList rosterPreferences() const { return m_rosterPreferences; }
    QVariantList awards() const { return m_awards; }
    QVariantList alerts() const { return m_alerts; }
    QVariantList spotHeatmap() const { return m_spotHeatmap; }
    QVariantList spotTimeline() const { return m_spotTimeline; }
    QVariantList spotPaths() const { return m_spotPaths; }
    QVariantList bandActivity() const { return m_bandActivity; }
    QVariantList bandActivityTimeline() const { return m_bandActivityTimeline; }
    QVariantMap bandActivitySummary() const { return m_bandActivitySummary; }
    int bandActivityWindowHours() const { return m_bandActivityWindowHours; }
    QVariantList rosterRules() const { return m_rosterRules; }
    QVariantMap statistics() const { return m_statistics; }
    QStringList availableBands() const { return m_availableBands; }
    QStringList availableModes() const { return m_availableModes; }
    QStringList availablePeriods() const;
    QStringList availableContinents() const { return m_availableContinents; }
    QStringList availableDxcc() const { return m_availableDxcc; }
    QStringList availableSources() const { return m_availableSources; }
    QStringList availableRosterStatuses() const;
    QStringList availableRosterHuntScopes() const;
    QStringList availableAwardPrograms() const;
    QStringList availableAwardGoals() const;
    QStringList availablePskDisplayModes() const;
    QStringList availableSpotAgeFilters() const;
    QStringList availableCorrelationFilters() const;
    QStringList availableRosterColumns() const;
    QStringList availableCallLookupProviders() const;
    QString bandFilter() const { return m_bandFilter; }
    QString modeFilter() const { return m_modeFilter; }
    QString periodFilter() const { return m_periodFilter; }
    QString continentFilter() const { return m_continentFilter; }
    QString dxccFilter() const { return m_dxccFilter; }
    QString sourceFilter() const { return m_sourceFilter; }
    bool cqOnly() const { return m_cqOnly; }
    QString rosterSort() const { return m_rosterSort; }
    bool rosterSortDescending() const { return m_rosterSortDescending; }
    QString rosterStatusFilter() const { return m_rosterStatusFilter; }
    QString rosterHuntScope() const { return m_rosterHuntScope; }
    int rosterRetentionMinutes() const { return m_rosterRetentionMinutes; }
    bool rosterCqOnly() const { return m_rosterCqOnly; }
    QString rosterTextFilter() const { return m_rosterTextFilter; }
    QString rosterTextMode() const { return m_rosterTextMode; }
    QString activeAwardProgram() const { return m_activeAwardProgram; }
    QString awardGoal() const { return m_awardGoal; }
    int gridPrecision() const { return m_gridPrecision; }
    int liveDecayMinutes() const { return m_liveDecayMinutes; }
    bool splitGridEnabled() const { return m_splitGridEnabled; }
    bool coveragePushPinsEnabled() const { return m_coveragePushPinsEnabled; }
    bool timeZoneOverlayEnabled() const { return m_timeZoneOverlayEnabled; }
    QString pskDisplayMode() const { return m_pskDisplayMode; }
    int pskOpacityPercent() const { return m_pskOpacityPercent; }
    QString spotAgeFilter() const { return m_spotAgeFilter; }
    QString spotCorrelationFilter() const { return m_spotCorrelationFilter; }
    QStringList rosterVisibleColumns() const { return m_rosterVisibleColumns; }
    QString callLookupProvider() const { return m_callLookupProvider; }
    bool alertNewGridEnabled() const { return m_alertNewGridEnabled; }
    bool alertNewDxccEnabled() const { return m_alertNewDxccEnabled; }
    bool alertCqEnabled() const { return m_alertCqEnabled; }
    QString alertCallPattern() const { return m_alertCallPattern; }
    bool workedLayerEnabled() const;
    bool confirmedLayerEnabled() const;
    bool liveLayerEnabled() const;
    bool activeLayerEnabled() const;
    bool missingLayerEnabled() const;
    bool pskLayerEnabled() const;
    bool liveEntryMatchesCurrentFilters(const QVariantMap& entry,
                                        qint64 dialFrequencyHz,
                                        const QString& band) const;
    bool loading() const { return m_loading; }
    QString sourcePath() const { return m_sourcePath; }
    QString databasePath() const { return m_databasePath; }
    int qsoCount() const { return m_qsoCount; }
    int workedGridCount() const { return m_workedGridCount; }
    int confirmedGridCount() const { return m_confirmedGridCount; }
    int activeGridCount() const { return m_activeGridCount; }
    int missingGridCount() const { return m_missingGridCount; }
    int liveSpotCount() const { return m_liveSpotCount; }
    int rosterCount() const { return m_roster.size(); }
    int rosterWantedCount() const { return m_rosterWantedCount; }
    int rosterNewCount() const { return m_rosterNewCount; }
    int rosterUnconfirmedCount() const { return m_rosterUnconfirmedCount; }
    int rosterPreferenceCount() const { return m_rosterPreferences.size(); }
    int unreadAlertCount() const { return m_unreadAlertCount; }
    QString selectedGrid() const { return m_selectedGrid; }
    QVariantMap selectedGridSummary() const { return m_selectedGridSummary; }
    QVariantList selectedGridLive() const { return m_selectedGridLive; }
    QVariantList selectedGridQsos() const { return m_selectedGridQsos; }
    bool gridDetailsLoading() const { return m_gridDetailsLoading; }

    void setBandFilter(const QString& value);
    void setModeFilter(const QString& value);
    void setPeriodFilter(const QString& value);
    void setContinentFilter(const QString& value);
    void setDxccFilter(const QString& value);
    void setSourceFilter(const QString& value);
    void setCqOnly(bool enabled);
    void setRosterSort(const QString& value);
    void setRosterSortDescending(bool descending);
    void setRosterStatusFilter(const QString& value);
    void setRosterHuntScope(const QString& value);
    void setRosterRetentionMinutes(int minutes);
    void setRosterCqOnly(bool enabled);
    void setRosterTextFilter(const QString& value);
    void setRosterTextMode(const QString& value);
    void setActiveAwardProgram(const QString& value);
    void setAwardGoal(const QString& value);
    void setGridPrecision(int precision);
    void setLiveDecayMinutes(int minutes);
    void setSplitGridEnabled(bool enabled);
    void setCoveragePushPinsEnabled(bool enabled);
    void setTimeZoneOverlayEnabled(bool enabled);
    void setPskDisplayMode(const QString& mode);
    void setPskOpacityPercent(int percent);
    void setSpotAgeFilter(const QString& value);
    void setSpotCorrelationFilter(const QString& value);
    void setBandActivityWindowHours(int hours);
    void setRosterVisibleColumns(const QStringList& columns);
    void setCallLookupProvider(const QString& provider);
    void setAlertNewGridEnabled(bool enabled);
    void setAlertNewDxccEnabled(bool enabled);
    void setAlertCqEnabled(bool enabled);
    void setAlertCallPattern(const QString& pattern);
    void setWorkedLayerEnabled(bool enabled);
    void setConfirmedLayerEnabled(bool enabled);
    void setLiveLayerEnabled(bool enabled);
    void setActiveLayerEnabled(bool enabled);
    void setMissingLayerEnabled(bool enabled);
    void setPskLayerEnabled(bool enabled);

    Q_INVOKABLE void reloadFromAdif(const QString& path);
    Q_INVOKABLE void appendAdifRecord(const QByteArray& record);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clearLiveSpots();
    Q_INVOKABLE void clearAlerts();
    Q_INVOKABLE void markAlertsRead();
    Q_INVOKABLE void setRosterCallWatched(const QString& call, bool watched);
    Q_INVOKABLE void setRosterCallIgnored(const QString& call, bool ignored);
    Q_INVOKABLE void setRosterDxccIgnored(const QString& dxcc, bool ignored);
    Q_INVOKABLE void removeRosterPreference(const QString& type,
                                            const QString& value);
    Q_INVOKABLE void clearRosterPreferences();
    Q_INVOKABLE void selectGrid(const QString& grid);
    Q_INVOKABLE void clearGridSelection();
    Q_INVOKABLE void ingestPskSpots(const QVariantList& rows,
                                     const QString& senderCall,
                                     const QString& senderGrid);
    Q_INVOKABLE void replacePskHeardBySpots(const QVariantList& rows,
                                            const QString& senderCall,
                                            const QString& senderGrid);
    Q_INVOKABLE void configurePskFeed(const QString& callsign, const QString& grid);
    Q_INVOKABLE void setRosterRule(const QString& type, const QString& value,
                                   const QString& action,
                                   const QString& band = {},
                                   const QString& mode = {});
    Q_INVOKABLE void removeRosterRule(const QString& type, const QString& value,
                                      const QString& band = {},
                                      const QString& mode = {});

    void ingestDecodeEntry(const QVariantMap& entry,
                           qint64 dialFrequencyHz,
                           const QString& band);

signals:
    void coverageChanged();
    void rosterChanged();
    void rosterPreferencesChanged();
    void awardsChanged();
    void alertsChanged();
    void statisticsChanged();
    void filtersChanged();
    void bandFilterChanged();
    void modeFilterChanged();
    void periodFilterChanged();
    void continentFilterChanged();
    void dxccFilterChanged();
    void sourceFilterChanged();
    void cqOnlyChanged();
    void rosterSortChanged();
    void rosterSortDescendingChanged();
    void rosterStatusFilterChanged();
    void rosterHuntScopeChanged();
    void rosterRetentionMinutesChanged();
    void rosterCqOnlyChanged();
    void rosterTextFilterChanged();
    void rosterTextModeChanged();
    void activeAwardProgramChanged();
    void awardGoalChanged();
    void gridPrecisionChanged();
    void liveDecayMinutesChanged();
    void splitGridEnabledChanged();
    void coveragePushPinsEnabledChanged();
    void timeZoneOverlayEnabledChanged();
    void pskDisplayModeChanged();
    void pskOpacityPercentChanged();
    void spotAgeFilterChanged();
    void spotCorrelationFilterChanged();
    void rosterVisibleColumnsChanged();
    void spotAnalyticsChanged();
    void bandActivityChanged();
    void bandActivityWindowHoursChanged();
    void rosterRulesChanged();
    void callLookupProviderChanged();
    void alertRulesChanged();
    void workedLayerEnabledChanged();
    void confirmedLayerEnabledChanged();
    void liveLayerEnabledChanged();
    void activeLayerEnabledChanged();
    void missingLayerEnabledChanged();
    void pskLayerEnabledChanged();
    void loadingChanged();
    void sourcePathChanged();
    void gridDetailsChanged();
    void gridDetailsLoadingChanged();

private slots:
    void scheduleQuery();
    void flushPendingLiveSpots();

private:
    struct QsoRecord {
        QString sourceKey;
        QString call;
        QString grid;
        QString grid4;
        QString grid6;
        QString band;
        QString mode;
        QString qsoDate;
        QString timeOn;
        QString source {QStringLiteral("ADIF")};
        QString dxcc;
        QString continent;
        QString state;
        QString county;
        QString potaReference;
        QString iotaReference;
        QString wpxPrefix;
        double frequencyMhz {0.0};
        qint64 qsoEpoch {0};
        int cqZone {0};
        int ituZone {0};
        bool confirmed {false};
        bool lotwConfirmed {false};
        bool eqslConfirmed {false};
        bool oqrs {false};
    };

    struct LiveSpot {
        QString uniqueKey;
        QString call;
        QString grid;
        QString grid4;
        QString grid6;
        QString band;
        QString mode;
        QString message;
        QString observedUtc;
        qint64 observedMs {0};
        qint64 frequencyHz {0};
        double distanceKm {-1.0};
        int snr {0};
        QString source;
        QString dxcc;
        QString continent;
        QString state;
        QString targetCall;
        QString activityType;
        QString receiverCall;
        QString receiverGrid;
        QString provider;
        QString direction {QStringLiteral("RX")};
        int cqZone {0};
        int ituZone {0};
        bool isCq {false};
    };

    struct PendingDecode {
        QVariantMap entry;
        qint64 dialFrequencyHz {0};
        QString band;
    };

    struct Snapshot {
        QVariantList coverage;
        QVariantList roster;
        QVariantList rosterPreferences;
        QVariantList awards;
        QVariantList alerts;
        QVariantList spotHeatmap;
        QVariantList spotTimeline;
        QVariantList spotPaths;
        QVariantList bandActivity;
        QVariantList bandActivityTimeline;
        QVariantMap bandActivitySummary;
        QVariantList rosterRules;
        QVariantMap statistics;
        QStringList bands {QStringLiteral("All")};
        QStringList modes {QStringLiteral("All")};
        QStringList continents {QStringLiteral("All")};
        QStringList dxcc {QStringLiteral("All")};
        QStringList sources {QStringLiteral("All")};
        int qsoCount {0};
        int workedGridCount {0};
        int confirmedGridCount {0};
        int activeGridCount {0};
        int missingGridCount {0};
        int liveSpotCount {0};
        int pskListenerCount {0};
        int rosterWantedCount {0};
        int rosterNewCount {0};
        int rosterUnconfirmedCount {0};
        int unreadAlertCount {0};
        QString error;
    };

    struct QueryOptions {
        QString band;
        QString mode;
        QString period;
        QString continent;
        QString dxcc;
        QString source;
        QString rosterSort;
        QString rosterStatus;
        QString rosterHuntScope;
        QString activeAwardProgram;
        QString awardGoal;
        int rosterRetentionMinutes {5};
        int gridPrecision {4};
        int liveDecayMinutes {15};
        bool cqOnly {false};
        bool rosterSortDescending {true};
        bool rosterCqOnly {false};
        bool splitGridEnabled {true};
        QString rosterText;
        QString rosterTextMode;
        bool pskLayerEnabled {true};
        QString pskDisplayMode {QStringLiteral("Overlay")};
        double pskOpacity {0.65};
        QString spotAgeFilter {QStringLiteral("15 min")};
        QString spotCorrelationFilter {QStringLiteral("All")};
        int bandActivityWindowHours {6};
    };

    struct AlertRules {
        bool newGridEnabled {true};
        bool newDxccEnabled {true};
        bool cqEnabled {true};
        QString callPattern;
    };

    struct GridDetails {
        QVariantMap summary;
        QVariantList live;
        QVariantList qsos;
        QString error;
    };

    static QList<QsoRecord> parseAdif(const QByteArray& data);
    static LiveSpot liveSpotFromEntry(const QVariantMap& entry,
                                      qint64 dialFrequencyHz,
                                      const QString& band);
    static Snapshot queryDatabase(const QString& databasePath,
                                  const QueryOptions& options);
    static GridDetails queryGridDetails(const QString& databasePath,
                                        const QString& grid);
    static bool importAdifIntoDatabase(const QString& databasePath,
                                       const QString& sourcePath,
                                       const QByteArray& data,
                                       const QString& fingerprint,
                                       QString* error);
    static bool appendQsoRecords(const QString& databasePath,
                                 const QList<QsoRecord>& records,
                                 QString* error);
    static bool appendLiveSpots(const QString& databasePath,
                                const QList<LiveSpot>& spots,
                                const AlertRules& rules,
                                QString* error);
    static bool clearLiveSpotRows(const QString& databasePath, QString* error);
    static bool clearPskHeardByRows(const QString& databasePath, QString* error);
    static bool clearAlertRows(const QString& databasePath, QString* error);
    static bool markAlertRowsRead(const QString& databasePath, QString* error);
    static bool updateRosterPreference(const QString& databasePath,
                                       const QString& call,
                                       bool watched,
                                       bool ignored,
                                       QString* error);
    static bool updateRosterIgnore(const QString& databasePath,
                                   const QString& type,
                                   const QString& value,
                                   bool ignored,
                                   QString* error);
    static bool removeRosterPreferenceRow(const QString& databasePath,
                                          const QString& type,
                                          const QString& value,
                                          QString* error);
    static bool clearRosterPreferenceRows(const QString& databasePath,
                                          QString* error);
    static bool updateRosterRuleRow(const QString& databasePath,
                                    const QString& type, const QString& value,
                                    const QString& action, const QString& band,
                                    const QString& mode, QString* error);
    static bool removeRosterRuleRow(const QString& databasePath,
                                    const QString& type, const QString& value,
                                    const QString& band, const QString& mode,
                                    QString* error);

    void queueSnapshotQuery(quint64 generation);
    void queuePskSpots(const QVariantList& rows,
                       const QString& senderCall,
                       const QString& senderGrid,
                       bool replaceHeardBySnapshot);
    void applySnapshot(quint64 generation, Snapshot snapshot);
    void applyGridDetails(quint64 generation, const QString& grid,
                          GridDetails details);
    void rebuildVisibleCoverage();
    void setLoading(bool loading);
    void setGridDetailsLoading(bool loading);
    void setOfflineMode(bool offline);
    void saveSetting(const QString& key, const QVariant& value) const;

    MapLayerModel* m_layerModel {nullptr};
    MapBaseMapService* m_baseMapService {nullptr};
    MapExternalOverlayService* m_externalOverlayService {nullptr};
    MapOperationsService* m_operationsService {nullptr};
    MapPskFeedService* m_pskFeedService {nullptr};
    QThreadPool m_workerPool;
    QTimer* m_queryTimer {nullptr};
    QTimer* m_liveFlushTimer {nullptr};
    QList<PendingDecode> m_pendingDecodes;
    QVariantList m_rawCoverage;
    QVariantList m_coverageCells;
    QVariantList m_roster;
    QVariantList m_rosterPreferences;
    QVariantList m_awards;
    QVariantList m_alerts;
    QVariantList m_spotHeatmap;
    QVariantList m_spotTimeline;
    QVariantList m_spotPaths;
    QVariantList m_bandActivity;
    QVariantList m_bandActivityTimeline;
    QVariantMap m_bandActivitySummary;
    QVariantList m_rosterRules;
    QVariantMap m_statistics;
    QVariantMap m_selectedGridSummary;
    QVariantList m_selectedGridLive;
    QVariantList m_selectedGridQsos;
    QStringList m_availableBands {QStringLiteral("All")};
    QStringList m_availableModes {QStringLiteral("All")};
    QStringList m_availableContinents {QStringLiteral("All")};
    QStringList m_availableDxcc {QStringLiteral("All")};
    QStringList m_availableSources {QStringLiteral("All")};
    QString m_bandFilter {QStringLiteral("All")};
    QString m_modeFilter {QStringLiteral("All")};
    QString m_periodFilter {QStringLiteral("All time")};
    QString m_continentFilter {QStringLiteral("All")};
    QString m_dxccFilter {QStringLiteral("All")};
    QString m_sourceFilter {QStringLiteral("All")};
    QString m_rosterSort {QStringLiteral("Time")};
    QString m_rosterStatusFilter {QStringLiteral("All")};
    QString m_rosterHuntScope {QStringLiteral("All time")};
    QString m_activeAwardProgram {QStringLiteral("None")};
    QString m_awardGoal {QStringLiteral("Confirmed")};
    QString m_sourcePath;
    QString m_databasePath;
    QString m_selectedGrid;
    bool m_cqOnly {false};
    bool m_rosterSortDescending {true};
    bool m_rosterCqOnly {false};
    QString m_rosterTextFilter;
    QString m_rosterTextMode {QStringLiteral("No filter")};
    bool m_loading {false};
    bool m_gridDetailsLoading {false};
    int m_rosterRetentionMinutes {5};
    int m_gridPrecision {4};
    int m_liveDecayMinutes {15};
    bool m_splitGridEnabled {true};
    bool m_coveragePushPinsEnabled {false};
    bool m_timeZoneOverlayEnabled {false};
    QString m_pskDisplayMode {QStringLiteral("Overlay")};
    int m_pskOpacityPercent {65};
    QString m_spotAgeFilter {QStringLiteral("15 min")};
    QString m_spotCorrelationFilter {QStringLiteral("All")};
    int m_bandActivityWindowHours {6};
    QStringList m_rosterVisibleColumns {
        QStringLiteral("Grid"), QStringLiteral("Band"), QStringLiteral("Mode"),
        QStringLiteral("SNR"), QStringLiteral("DXCC"), QStringLiteral("Age")};
    QString m_callLookupProvider {QStringLiteral("QRZ")};
    bool m_alertNewGridEnabled {true};
    bool m_alertNewDxccEnabled {true};
    bool m_alertCqEnabled {true};
    QString m_alertCallPattern;
    int m_qsoCount {0};
    int m_workedGridCount {0};
    int m_confirmedGridCount {0};
    int m_activeGridCount {0};
    int m_missingGridCount {0};
    int m_liveSpotCount {0};
    int m_rosterWantedCount {0};
    int m_rosterNewCount {0};
    int m_rosterUnconfirmedCount {0};
    int m_unreadAlertCount {0};
    std::atomic<quint64> m_queryGeneration {0};
    std::atomic<quint64> m_importGeneration {0};
    std::atomic<quint64> m_gridDetailsGeneration {0};
};
