pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    signal closeRequested()

    property var dragTarget: null
    property var stations: []
    property var sessions: []
    property var selectedMessages: []
    property var broadcasts: []
    property var alerts: []
    property var alertTagList: []
    property var mailbox: []
    property var relayQueue: []
    property var formTemplates: []
    property var forms: []
    property var fileTransfers: []
    property var receivedFiles: []
    property var bulletins: []
    property var qsoLog: []
    property var logbookOutbox: []
    property var contactHistory: []
    property var selectedContactTimeline: []
    property var pingLog: []
    property var pathReports: []
    property var beaconHistory: []
    property var clusterLastHeard: []
    property var pathAnalysis: ({})
    property var statistics: ({})
    property var storeAudit: ({})
    property var clusterConfigState: ({})
    property var cannedMessages: []
    property var customCannedMessages: []
    property var qsySlots: []
    property var qsyPlan: ({})
    property var frequencyPresetList: []
    property var allowedQsyRangeList: []
    property var frequencyScheduleList: []
    property var presenceState: ({})
    property var qsoAutomationState: ({})
    property var blockedCalls: []
    property int selectedSessionId: ft2Link ? ft2Link.activeSessionId : 0
    property string selectedRemoteCall: ""
    property string selectedSessionStateName: ""
    property var lastHelloBytes: null
    property bool cqOnly: false
    property int formTemplateIndex: 0
    property bool preferW2300: settingBool("uiFt2LinkPreferW2300", true)
    property bool robustMode: settingBool("uiFt2LinkRobustMode", false)
    property int beaconIntervalSeconds: settingInt("uiFt2LinkBeaconIntervalSeconds", 180, 180, 600)
    property int toolPageIndex: 0
    property int qsySlotIndex: settingInt("uiFt2LinkQsySlotIndex", 0, 0, 9)
    property int cqSlotIndex: settingInt("uiFt2LinkCqSlotIndex", 0, 0, 9)
    property int cqSlotWaitSeconds: settingInt("uiFt2LinkCqSlotWaitSeconds", 120, 60, 3600)
    property int cqTypeIndex: settingInt("uiFt2LinkCqTypeIndex", 0, 0, 5)
    property string cqLocator: settingString("uiFt2LinkCqLocator", "")
    property int qsyCallingFrequencyHz: settingInt("uiFt2LinkCallingFrequencyHz", 0, 0, 999999999)
    property int stationPaneWidth: settingInt("uiFt2LinkStationPaneWidth", 220, 160, 420)
    property int sessionPaneWidth: settingInt("uiFt2LinkSessionPaneWidth", 170, 130, 340)
    property string selectedContactCall: ""
    property string pathFilterCall: ""
    property string pathFilterGrid: ""
    property string logExportText: ""
    property string databaseActionText: ""
    property string typingSummaryText: ""
    property bool chatScrollPinned: true
    property bool chatUnreadBelow: false
    property bool chatUnreadPulse: false
    property bool stationHistoryMode: false
    property bool skipCqSlot: settingBool("uiFt2LinkSkipCqSlot", false)
    property double cqSlotWaitUntilMs: 0
    property string profileName: settingString("uiFt2LinkProfileName", "")
    property string profileQth: settingString("uiFt2LinkProfileQth", "")
    property string profileEmail: settingString("uiFt2LinkProfileEmail", "")
    property string profileRig: settingString("uiFt2LinkProfileRig", "")
    property string profileAntenna: settingString("uiFt2LinkProfileAntenna", "")
    property string profilePower: settingString("uiFt2LinkProfilePower", "")
    property string profileIce: settingString("uiFt2LinkProfileIce", "")
    property string profileGps: settingString("uiFt2LinkProfileGps", "")
    property string clusterSharePath: settingString("uiFt2LinkClusterSharePath", "")
    property bool clusterAutoSync: settingBool("uiFt2LinkClusterAutoSync", false)
    property int clusterAutoSyncSeconds: settingInt("uiFt2LinkClusterAutoSyncSeconds", 60, 30, 900)
    property double clusterLastAutoSyncMs: 0
    property string clusterSyncStatus: ""
    property bool emailGatewayEnabled: settingBool("uiFt2LinkEmailGatewayEnabled", false)
    property string emailGatewayHost: settingString("uiFt2LinkEmailGatewayHost", "")
    property int emailGatewayPort: settingInt("uiFt2LinkEmailGatewayPort", 587, 1, 65535)
    property int emailGatewaySecurityIndex: settingInt("uiFt2LinkEmailGatewaySecurityIndex", 0, 0, 2)
    property string emailGatewayUsername: settingString("uiFt2LinkEmailGatewayUsername", "")
    property string emailGatewayFrom: settingString("uiFt2LinkEmailGatewayFrom", "")
    property string emailGatewayStatus: ""
    property var emailGatewayRequestStates: ({})
    property string checkInCity: settingString("uiFt2LinkCheckInCity", "")
    property string checkInRegion: settingString("uiFt2LinkCheckInRegion", "")
    property string checkInChannel: settingString("uiFt2LinkCheckInChannel", "HF")
    property double uiNowMs: Date.now()
    readonly property bool selectedSessionConnected: selectedSessionStateName === "Connected"
    readonly property var cqTypeOptions: ["CQ", "CHAT", "NET", "EMCOMM", "TEST", "QSY"]
    readonly property var emailGatewaySecurityOptions: ["STARTTLS", "TLS", "NONE"]

    readonly property color panelBg: Qt.rgba(0.025, 0.035, 0.045, 0.94)
    readonly property color railBg: Qt.rgba(0.055, 0.070, 0.085, 0.96)
    readonly property color rowHover: Qt.rgba(0.18, 0.62, 0.72, 0.16)
    readonly property color rowSelect: Qt.rgba(0.10, 0.42, 0.50, 0.36)
    readonly property color borderSoft: Qt.rgba(0.42, 0.58, 0.64, 0.30)
    readonly property color cyan: "#49d7e8"
    readonly property color green: "#78d77c"
    readonly property color amber: "#e7b85c"
    readonly property color red: "#ef6f6c"
    readonly property color textPrimary: "#e8eef2"
    readonly property color textSecondary: "#93a7b0"
    readonly property string mono: typeof decodiumMonoFontFamily !== "undefined" ? decodiumMonoFontFamily : "monospace"

    color: panelBg
    border.color: Qt.rgba(0.30, 0.80, 0.90, 0.42)
    border.width: 1
    radius: 6
    clip: true
    implicitWidth: 720
    implicitHeight: 430

    onPreferW2300Changed: {
        persistSetting("uiFt2LinkPreferW2300", preferW2300)
        applyCapabilities()
    }
    onRobustModeChanged: {
        persistSetting("uiFt2LinkRobustMode", robustMode)
        applyCapabilities()
    }
    onBeaconIntervalSecondsChanged: {
        persistSetting("uiFt2LinkBeaconIntervalSeconds", beaconIntervalSeconds)
    }
    onQsySlotIndexChanged: {
        persistSetting("uiFt2LinkQsySlotIndex", qsySlotIndex)
    }
    onQsyCallingFrequencyHzChanged: persistSetting("uiFt2LinkCallingFrequencyHz", qsyCallingFrequencyHz)
    onCqSlotIndexChanged: persistSetting("uiFt2LinkCqSlotIndex", cqSlotIndex)
    onCqSlotWaitSecondsChanged: persistSetting("uiFt2LinkCqSlotWaitSeconds", cqSlotWaitSeconds)
    onCqTypeIndexChanged: persistSetting("uiFt2LinkCqTypeIndex", cqTypeIndex)
    onCqLocatorChanged: persistSetting("uiFt2LinkCqLocator", cqLocator)
    onStationPaneWidthChanged: persistSetting("uiFt2LinkStationPaneWidth", stationPaneWidth)
    onSessionPaneWidthChanged: persistSetting("uiFt2LinkSessionPaneWidth", sessionPaneWidth)
    onSkipCqSlotChanged: persistSetting("uiFt2LinkSkipCqSlot", skipCqSlot)
    onProfileNameChanged: { persistSetting("uiFt2LinkProfileName", profileName); syncLocalStation() }
    onProfileQthChanged: { persistSetting("uiFt2LinkProfileQth", profileQth); syncLocalStation() }
    onProfileEmailChanged: { persistSetting("uiFt2LinkProfileEmail", profileEmail); syncLocalStation() }
    onProfileRigChanged: { persistSetting("uiFt2LinkProfileRig", profileRig); syncLocalStation() }
    onProfileAntennaChanged: { persistSetting("uiFt2LinkProfileAntenna", profileAntenna); syncLocalStation() }
    onProfilePowerChanged: { persistSetting("uiFt2LinkProfilePower", profilePower); syncLocalStation() }
    onProfileIceChanged: { persistSetting("uiFt2LinkProfileIce", profileIce); syncLocalStation() }
    onProfileGpsChanged: { persistSetting("uiFt2LinkProfileGps", profileGps); syncLocalStation() }
    onClusterSharePathChanged: persistSetting("uiFt2LinkClusterSharePath", clusterSharePath)
    onClusterAutoSyncChanged: {
        persistSetting("uiFt2LinkClusterAutoSync", clusterAutoSync)
        clusterLastAutoSyncMs = 0
        clusterSyncStatus = clusterAutoSync ? "AUTO waiting" : "AUTO off"
    }
    onClusterAutoSyncSecondsChanged: persistSetting("uiFt2LinkClusterAutoSyncSeconds", clusterAutoSyncSeconds)
    onEmailGatewayEnabledChanged: persistSetting("uiFt2LinkEmailGatewayEnabled", emailGatewayEnabled)
    onEmailGatewayHostChanged: persistSetting("uiFt2LinkEmailGatewayHost", emailGatewayHost)
    onEmailGatewayPortChanged: persistSetting("uiFt2LinkEmailGatewayPort", emailGatewayPort)
    onEmailGatewaySecurityIndexChanged: persistSetting("uiFt2LinkEmailGatewaySecurityIndex", emailGatewaySecurityIndex)
    onEmailGatewayUsernameChanged: persistSetting("uiFt2LinkEmailGatewayUsername", emailGatewayUsername)
    onEmailGatewayFromChanged: persistSetting("uiFt2LinkEmailGatewayFrom", emailGatewayFrom)
    onCheckInCityChanged: persistSetting("uiFt2LinkCheckInCity", checkInCity)
    onCheckInRegionChanged: persistSetting("uiFt2LinkCheckInRegion", checkInRegion)
    onCheckInChannelChanged: persistSetting("uiFt2LinkCheckInChannel", checkInChannel)

    function coerceBool(value, fallback) {
        if (value === undefined || value === null)
            return fallback
        if (typeof value === "boolean")
            return value
        var text = String(value).trim().toLowerCase()
        if (text === "true" || text === "1" || text === "yes" || text === "on")
            return true
        if (text === "false" || text === "0" || text === "no" || text === "off")
            return false
        return fallback
    }

    function settingBool(key, fallback) {
        if (bridge && typeof bridge.getSetting === "function")
            return coerceBool(bridge.getSetting(key, fallback), fallback)
        return fallback
    }

    function settingInt(key, fallback, minValue, maxValue) {
        var value = fallback
        if (bridge && typeof bridge.getSetting === "function")
            value = Number(bridge.getSetting(key, fallback))
        if (!isFinite(value))
            value = fallback
        value = Math.round(value)
        if (minValue !== undefined)
            value = Math.max(minValue, value)
        if (maxValue !== undefined)
            value = Math.min(maxValue, value)
        return value
    }

    function settingString(key, fallback) {
        if (bridge && typeof bridge.getSetting === "function")
            return String(bridge.getSetting(key, fallback))
        return fallback
    }

    function persistSetting(key, value) {
        if (bridge && typeof bridge.setSetting === "function")
            bridge.setSetting(key, value)
    }

    function nowMs() {
        return root.uiNowMs
    }

    function syncLocalStation() {
        if (!ft2Link)
            return
        var call = bridge && bridge.callsign ? String(bridge.callsign) : ""
        var grid = bridge && bridge.grid ? String(bridge.grid) : ""
        ft2Link.setLocalStation(call, grid, profileName.trim())
        ft2Link.setLocalOperatorProfile(profileQth.trim(),
                                        profileEmail.trim(),
                                        profileIce.trim(),
                                        profileRig.trim(),
                                        profileAntenna.trim(),
                                        profilePower.trim(),
                                        profileGps.trim())
    }

    function applyCapabilities() {
        if (!ft2Link)
            return
        ft2Link.setLocalCapabilities(true, true, true, true,
                                     preferW2300 ? 2 : 1,
                                     robustMode ? 1 : 0)
    }

    function refreshStations() {
        if (!ft2Link) {
            stations = []
            return
        }
        stations = ft2Link.activeStations(nowMs(), 300000, cqOnly)
    }

    function refreshSessions() {
        if (!ft2Link) {
            sessions = []
            selectedMessages = []
            return
        }
        sessions = ft2Link.sessions()
        if (selectedSessionId === 0 && sessions.length > 0)
            selectSession(Number(sessions[sessions.length - 1].sessionId))
        else {
            updateSelectedSessionFromSessions()
            refreshMessages()
        }
    }

    function refreshMessages() {
        var previousCount = selectedMessages.length
        var wasAtEnd = messageListAtEnd()
        selectedMessages = ft2Link && selectedSessionId > 0 ? ft2Link.messages(selectedSessionId) : []
        refreshQsyPlan()
        if (selectedMessages.length === 0) {
            chatUnreadBelow = false
            chatScrollPinned = true
            return
        }
        if (wasAtEnd || selectedMessages.length <= previousCount) {
            chatUnreadBelow = false
            chatScrollPinned = true
            Qt.callLater(scrollChatToEnd)
        } else if (selectedMessages.length > previousCount) {
            chatUnreadBelow = true
            chatScrollPinned = false
        }
    }

    function refreshBroadcasts() {
        broadcasts = ft2Link ? ft2Link.broadcasts() : []
    }

    function refreshAlerts() {
        alerts = ft2Link ? ft2Link.alertEvents() : []
    }

    function refreshAlertTags() {
        alertTagList = ft2Link && typeof ft2Link.customAlertTags === "function"
                       ? ft2Link.customAlertTags()
                       : []
    }

    function refreshMailbox() {
        mailbox = ft2Link ? ft2Link.mailbox() : []
        relayQueue = ft2Link && typeof ft2Link.relayQueue === "function"
                     ? ft2Link.relayQueue(nowMs())
                     : []
    }

    function refreshFormTemplates() {
        formTemplates = ft2Link ? ft2Link.formTemplates() : []
        if (formTemplateIndex >= formTemplates.length)
            formTemplateIndex = 0
    }

    function refreshForms() {
        forms = ft2Link ? ft2Link.forms() : []
    }

    function refreshFileTransfers() {
        fileTransfers = ft2Link ? ft2Link.fileTransfers() : []
    }

    function refreshReceivedFiles() {
        receivedFiles = ft2Link ? ft2Link.receivedFiles() : []
    }

    function refreshBulletins() {
        bulletins = ft2Link ? ft2Link.bulletins() : []
    }

    function refreshQsoLog() {
        qsoLog = ft2Link ? ft2Link.qsoLog() : []
    }

    function refreshLogbookOutbox() {
        logbookOutbox = ft2Link && typeof ft2Link.logbookOutbox === "function"
                        ? ft2Link.logbookOutbox()
                        : []
    }

    function refreshContactHistory() {
        contactHistory = ft2Link ? ft2Link.contactHistory() : []
    }

    function refreshContactTimeline() {
        selectedContactTimeline = ft2Link && selectedContactCall.length > 0
                                  ? ft2Link.contactTimeline(selectedContactCall)
                                  : []
    }

    function refreshPingLog() {
        pingLog = ft2Link ? ft2Link.pingLog() : []
    }

    function refreshPathReports() {
        pathReports = ft2Link ? ft2Link.pathReports() : []
    }

    function refreshBeaconHistory() {
        beaconHistory = ft2Link && typeof ft2Link.beaconHistory === "function"
                        ? ft2Link.beaconHistory()
                        : []
    }

    function refreshClusterLastHeard() {
        clusterConfigState = ft2Link && typeof ft2Link.clusterConfig === "function"
                             ? ft2Link.clusterConfig()
                             : ({})
        clusterLastHeard = ft2Link && typeof ft2Link.clusterLastHeard === "function"
                           ? ft2Link.clusterLastHeard()
                           : []
    }

    function updateClusterFromRig() {
        if (!ft2Link || typeof ft2Link.configureCluster !== "function")
            return
        ft2Link.configureCluster(true, "", "", currentDialFrequencyHz())
        refreshClusterLastHeard()
    }

    function refreshPathAnalysis() {
        pathAnalysis = ft2Link ? ft2Link.pathAnalysis(pathFilterCall, pathFilterGrid) : ({})
    }

    function refreshStatistics() {
        statistics = ft2Link ? ft2Link.statistics() : ({})
    }

    function refreshStoreAudit() {
        storeAudit = ft2Link ? ft2Link.localStoreAudit() : ({})
    }

    function refreshCannedMessages() {
        cannedMessages = ft2Link ? ft2Link.cannedMessages() : []
        customCannedMessages = ft2Link && typeof ft2Link.customCannedMessages === "function"
                               ? ft2Link.customCannedMessages()
                               : []
    }

    function refreshQsySlots() {
        qsySlots = ft2Link ? ft2Link.qsySlots(750, 5) : []
        if (qsySlotIndex >= qsySlots.length)
            qsySlotIndex = 0
        if (cqSlotIndex >= qsySlots.length)
            cqSlotIndex = 0
    }

    function refreshFrequencyPlan() {
        frequencyPresetList = ft2Link && typeof ft2Link.frequencyPresets === "function"
                              ? ft2Link.frequencyPresets()
                              : []
        allowedQsyRangeList = ft2Link && typeof ft2Link.allowedQsyRanges === "function"
                              ? ft2Link.allowedQsyRanges()
                              : []
        frequencyScheduleList = ft2Link && typeof ft2Link.frequencySchedule === "function"
                                ? ft2Link.frequencySchedule()
                                : []
    }

    function refreshPresence() {
        presenceState = ft2Link && typeof ft2Link.presence === "function"
                        ? ft2Link.presence()
                        : ({})
    }

    function refreshQsoAutomation() {
        qsoAutomationState = ft2Link && typeof ft2Link.qsoAutomation === "function"
                             ? ft2Link.qsoAutomation()
                             : ({})
    }

    function refreshBlockedCalls() {
        blockedCalls = ft2Link && typeof ft2Link.blockedCalls === "function"
                       ? ft2Link.blockedCalls()
                       : []
    }

    function refreshTypingIndicators() {
        typingSummaryText = ft2Link && typeof ft2Link.typingSummary === "function"
                            ? String(ft2Link.typingSummary(nowMs()))
                            : ""
    }

    function logbookStateCount(stateName) {
        var wanted = String(stateName || "")
        var count = 0
        for (var i = 0; i < logbookOutbox.length; ++i) {
            if (String(logbookOutbox[i].state || "") === wanted)
                ++count
        }
        return count
    }

    function queueStatusLine() {
        if (!ft2Link)
            return "QUEUE --"
        var parts = []
        var queued = logbookStateCount("Queued")
        var submitted = logbookStateCount("Submitted")
        var failed = logbookStateCount("Failed")
        if (ft2Link.relayQueueCount > 0)
            parts.push("RLY " + ft2Link.relayQueueCount)
        if (ft2Link.mailboxUnreadCount > 0)
            parts.push("UNREAD " + ft2Link.mailboxUnreadCount)
        if (queued > 0 || submitted > 0 || failed > 0)
            parts.push("LBQ " + queued + "/" + submitted + "/" + failed)
        if (ft2Link.alertCount > 0)
            parts.push("ALERT " + ft2Link.alertCount)
        if (parts.length === 0)
            parts.push("clear")
        return "QUEUE " + parts.join("  ")
    }

    function queueStatusColor() {
        if (ft2Link && (ft2Link.alertCount > 0 || logbookStateCount("Failed") > 0))
            return root.red
        if (ft2Link && (ft2Link.relayQueueCount > 0
                        || ft2Link.mailboxUnreadCount > 0
                        || logbookStateCount("Submitted") > 0
                        || logbookStateCount("Queued") > 0))
            return root.amber
        return root.textSecondary
    }

    function queueStatusActive() {
        return !!ft2Link && (ft2Link.alertCount > 0
                             || ft2Link.relayQueueCount > 0
                             || ft2Link.mailboxUnreadCount > 0
                             || logbookStateCount("Submitted") > 0
                             || logbookStateCount("Queued") > 0
                             || logbookStateCount("Failed") > 0)
    }

    function rfStatusLine() {
        if (!ft2Link)
            return "RF --"
        var plan = ft2Link.lastRadioTxPlan || ({})
        var kind = String(plan.kind || plan.frameKind || "")
        var profile = String(plan.profileName || plan.profile || "")
        var state = String(ft2Link.transportState || "Idle")
        var detail = kind.length > 0 ? kind : state
        if (profile.length > 0)
            detail += " " + profile
        var bursts = Number(plan.bursts || plan.burstCount || 0)
        if (isFinite(bursts) && bursts > 0)
            detail += " " + bursts + "b"
        return "RF " + detail
    }

    function globalErrorLine() {
        if (!ft2Link || String(ft2Link.lastError || "").length === 0)
            return ""
        return "ERR " + String(ft2Link.lastError)
    }

    function loadPresenceEditor() {
        refreshPresence()
        awayCheck.checked = !!presenceState.awayEnabled
        awayQsyCheck.checked = !!presenceState.awayAcceptsQsy
        awayMessageText.text = String(presenceState.awayMessage || "QRX DE <MYCALL>")
        welcomeCheck.checked = !!presenceState.welcomeEnabled
        welcomeMessageText.text = String(presenceState.welcomeMessage || "HELLO <CALL> DE <MYCALL>")
        autoReplyCheck.checked = !!presenceState.autoReplyEnabled
        autoAwayCheck.checked = !!presenceState.autoAwayEnabled
        autoAwayMinutesText.text = String(presenceState.autoAwayMinutes || 10)
        refreshQsoAutomation()
        callIdIntervalText.text = String(qsoAutomationState.callIdIntervalMinutes || 0)
        autoDisconnectText.text = String(qsoAutomationState.autoDisconnectMinutes || 0)
        incomingPingCheck.checked = qsoAutomationState.incomingPingsEnabled !== false
        lastHeardPeekingCheck.checked = qsoAutomationState.lastHeardPeekingEnabled !== false
        lastConnectionsPeekingCheck.checked = qsoAutomationState.lastConnectionsPeekingEnabled !== false
        parkedVmailPeekingCheck.checked = qsoAutomationState.parkedVmailPeekingEnabled !== false
        vmailParkingCheck.checked = qsoAutomationState.vmailParkingEnabled !== false
        snrReportCheck.checked = qsoAutomationState.snrReportSendingEnabled !== false
        verboseSnrAcceptCheck.checked = !!qsoAutomationState.verboseSnrAutoAcceptEnabled
        infoInquireCheck.checked = qsoAutomationState.infoInquireEnabled !== false
    }

    function savePresence() {
        if (!ft2Link || typeof ft2Link.configurePresence !== "function")
            return
        var result = ft2Link.configurePresence(awayCheck.checked,
                                               awayQsyCheck.checked,
                                               awayMessageText.text,
                                               welcomeCheck.checked,
                                               welcomeMessageText.text)
        if (typeof ft2Link.configureAutoReply === "function")
            result = ft2Link.configureAutoReply(autoReplyCheck.checked)
        if (typeof ft2Link.configureAutoAway === "function")
            result = ft2Link.configureAutoAway(autoAwayCheck.checked,
                                               Number(autoAwayMinutesText.text || 10),
                                               nowMs())
        databaseActionText = prettyJson(result)
        refreshPresence()
        refreshStatistics()
        refreshStoreAudit()
        awayMessageText.text = String(presenceState.awayMessage || "QRX DE <MYCALL>")
        welcomeMessageText.text = String(presenceState.welcomeMessage || "HELLO <CALL> DE <MYCALL>")
        autoReplyCheck.checked = !!presenceState.autoReplyEnabled
        autoAwayCheck.checked = !!presenceState.autoAwayEnabled
        autoAwayMinutesText.text = String(presenceState.autoAwayMinutes || 10)
        root.saveQsoAutomation()
    }

    function saveQsoAutomation() {
        if (!ft2Link || typeof ft2Link.configureQsoAutomation !== "function")
            return
        var result = ft2Link.configureQsoAutomation(Number(callIdIntervalText.text || 0),
                                                    Number(autoDisconnectText.text || 0))
        if (typeof ft2Link.configureIncomingPings === "function")
            result = ft2Link.configureIncomingPings(incomingPingCheck.checked)
        if (typeof ft2Link.configureLastHeardPeeking === "function")
            result = ft2Link.configureLastHeardPeeking(lastHeardPeekingCheck.checked)
        if (typeof ft2Link.configureLastConnectionsPeeking === "function")
            result = ft2Link.configureLastConnectionsPeeking(lastConnectionsPeekingCheck.checked)
        if (typeof ft2Link.configureParkedVmailPeeking === "function")
            result = ft2Link.configureParkedVmailPeeking(parkedVmailPeekingCheck.checked)
        if (typeof ft2Link.configureVmailParking === "function")
            result = ft2Link.configureVmailParking(vmailParkingCheck.checked)
        if (typeof ft2Link.configureSnrReportSending === "function")
            result = ft2Link.configureSnrReportSending(snrReportCheck.checked)
        if (typeof ft2Link.configureVerboseSnrAutoAccept === "function")
            result = ft2Link.configureVerboseSnrAutoAccept(verboseSnrAcceptCheck.checked)
        if (typeof ft2Link.configureInfoInquire === "function")
            result = ft2Link.configureInfoInquire(infoInquireCheck.checked)
        databaseActionText = prettyJson(result)
        refreshQsoAutomation()
        refreshStatistics()
        refreshStoreAudit()
        callIdIntervalText.text = String(qsoAutomationState.callIdIntervalMinutes || 0)
        autoDisconnectText.text = String(qsoAutomationState.autoDisconnectMinutes || 0)
        incomingPingCheck.checked = qsoAutomationState.incomingPingsEnabled !== false
        lastHeardPeekingCheck.checked = qsoAutomationState.lastHeardPeekingEnabled !== false
        lastConnectionsPeekingCheck.checked = qsoAutomationState.lastConnectionsPeekingEnabled !== false
        parkedVmailPeekingCheck.checked = qsoAutomationState.parkedVmailPeekingEnabled !== false
        vmailParkingCheck.checked = qsoAutomationState.vmailParkingEnabled !== false
        snrReportCheck.checked = qsoAutomationState.snrReportSendingEnabled !== false
        verboseSnrAcceptCheck.checked = !!qsoAutomationState.verboseSnrAutoAcceptEnabled
        infoInquireCheck.checked = qsoAutomationState.infoInquireEnabled !== false
    }

    function applyPrivacyPreset(preset) {
        if (!ft2Link || typeof ft2Link.applyPrivacyPreset !== "function")
            return
        var result = ft2Link.applyPrivacyPreset(String(preset || "CONTROL"))
        databaseActionText = prettyJson(result)
        refreshQsoAutomation()
        refreshStatistics()
        refreshStoreAudit()
        incomingPingCheck.checked = qsoAutomationState.incomingPingsEnabled !== false
        lastHeardPeekingCheck.checked = qsoAutomationState.lastHeardPeekingEnabled !== false
        lastConnectionsPeekingCheck.checked = qsoAutomationState.lastConnectionsPeekingEnabled !== false
        parkedVmailPeekingCheck.checked = qsoAutomationState.parkedVmailPeekingEnabled !== false
        vmailParkingCheck.checked = qsoAutomationState.vmailParkingEnabled !== false
        snrReportCheck.checked = qsoAutomationState.snrReportSendingEnabled !== false
        infoInquireCheck.checked = qsoAutomationState.infoInquireEnabled !== false
    }

    function privacyPresetName() {
        return String(qsoAutomationState.privacyPreset || "CUSTOM")
    }

    function privacySummaryText() {
        return String(qsoAutomationState.privacySummary || "Privacy custom")
    }

    function saveBlockedCalls() {
        if (!ft2Link || typeof ft2Link.setBlockedCalls !== "function")
            return
        var result = ft2Link.setBlockedCalls(blockedCallsText.text)
        databaseActionText = prettyJson(result)
        if (result && result.text !== undefined)
            blockedCallsText.text = String(result.text)
        refreshBlockedCalls()
        refreshStations()
        refreshStatistics()
        refreshStoreAudit()
    }

    function addBlockedCallFromEditor() {
        if (!ft2Link || typeof ft2Link.addBlockedCall !== "function")
            return
        var call = blockedCallText.text.trim().toUpperCase()
        if (call.length === 0)
            return
        var result = ft2Link.addBlockedCall(call)
        databaseActionText = prettyJson(result)
        blockedCallText.text = ""
        if (result && result.text !== undefined)
            blockedCallsText.text = String(result.text)
        refreshBlockedCalls()
        refreshStations()
        refreshStatistics()
        refreshStoreAudit()
    }

    function deleteBlockedCall(call) {
        if (!ft2Link || typeof ft2Link.deleteBlockedCall !== "function")
            return
        var result = ft2Link.deleteBlockedCall(String(call || ""))
        databaseActionText = prettyJson(result)
        if (result && result.text !== undefined)
            blockedCallsText.text = String(result.text)
        refreshBlockedCalls()
        refreshStations()
        refreshStatistics()
        refreshStoreAudit()
    }

    function clearBlockedCalls() {
        if (!ft2Link || typeof ft2Link.clearBlockedCalls !== "function")
            return
        var result = ft2Link.clearBlockedCalls()
        databaseActionText = prettyJson(result)
        blockedCallsText.text = ""
        refreshBlockedCalls()
        refreshStations()
        refreshStatistics()
        refreshStoreAudit()
    }

    function latestBroadcastLine() {
        if (broadcasts.length === 0)
            return ""
        var item = broadcasts[broadcasts.length - 1]
        var prefix = item.alert ? "ALERT " : "BCAST "
        return prefix + String(item.fromCall || "--") + ": " + String(item.text || "")
    }

    function latestMailboxLine() {
        if (mailbox.length === 0)
            return ""
        var item = mailbox[mailbox.length - 1]
        var peer = String(item.direction || "") === "Incoming"
                   ? String(item.fromCall || "--")
                   : String(item.toCall || "--")
        return "MAIL " + String(item.state || "--") + " " + peer + ": "
               + String(item.subject || "")
    }

    function latestFormLine() {
        if (forms.length === 0)
            return ""
        var item = forms[forms.length - 1]
        var peer = String(item.direction || "") === "Incoming"
                   ? String(item.fromCall || "--")
                   : String(item.toCall || "--")
        return "FORM " + String(item.state || "--") + " "
               + String(item.formType || "--") + " " + peer
    }

    function latestFileLine() {
        if (fileTransfers.length === 0)
            return ""
        var item = fileTransfers[fileTransfers.length - 1]
        var peer = String(item.direction || "") === "Incoming"
                   ? String(item.fromCall || "--")
                   : String(item.toCall || "--")
        return "FILE " + String(item.state || "--") + " " + peer + ": "
               + String(item.fileName || "")
    }

    function latestBulletinLine() {
        if (bulletins.length === 0)
            return ""
        var item = bulletins[bulletins.length - 1]
        var peer = String(item.direction || "") === "Incoming"
                   ? String(item.fromCall || "--")
                   : String(item.group || "ALL")
        return "BBS " + String(item.state || "--") + " " + peer + ": "
               + String(item.title || "")
    }

    function latestQsoLine() {
        if (qsoLog.length === 0)
            return ""
        var item = qsoLog[0]
        return "QSO " + String(item.remoteCall || "--") + " "
               + String(item.state || "--") + " " + String(item.profileName || "--")
    }

    function latestContactLine() {
        if (contactHistory.length === 0)
            return ""
        var item = contactHistory[0]
        var tag = String(item.tag || "")
        return "CALL " + String(item.call || "--")
               + (tag.length > 0 ? " [" + tag + "]" : "") + " "
               + String(item.lastEvent || "--") + " qso "
               + String(item.qsoCount || 0)
    }

    function latestPingLine() {
        if (pingLog.length === 0)
            return ""
        var item = pingLog[0]
        var rtt = Number(item.rttMs || 0)
        return "PING " + String(item.remoteCall || "--") + " "
               + String(item.state || "--")
               + (rtt > 0 ? " " + String(rtt) + " ms" : "")
    }

    function statValue(key, fallback) {
        if (!statistics)
            return fallback
        var value = statistics[key]
        return value === undefined || value === null ? fallback : value
    }

    function statCount(key) {
        var value = Number(statValue(key, 0))
        return isFinite(value) ? String(Math.round(value)) : "0"
    }

    function pathValue(key, fallback) {
        if (!pathAnalysis)
            return fallback
        var value = pathAnalysis[key]
        return value === undefined || value === null ? fallback : value
    }

    function pathCount(key) {
        var value = Number(pathValue(key, 0))
        return isFinite(value) ? String(Math.round(value)) : "0"
    }

    function pathAverage(key) {
        var value = Number(pathValue(key, NaN))
        return isFinite(value) ? value.toFixed(1) : "--"
    }

    function twoDigit(value) {
        var number = Math.max(0, Math.min(99, Math.round(Number(value || 0))))
        return number < 10 ? "0" + String(number) : String(number)
    }

    function applyPathFilter(call, grid) {
        pathFilterCall = String(call || "").trim().toUpperCase()
        pathFilterGrid = String(grid || "").trim().toUpperCase()
        refreshPathAnalysis()
    }

    function clearPathFilter() {
        pathFilterCall = ""
        pathFilterGrid = ""
        refreshPathAnalysis()
    }

    function receivedFileDate(item) {
        return String(item.receivedUtc || "--")
    }

    function prettyJson(value) {
        try {
            return JSON.stringify(value || {}, null, 2)
        } catch (error) {
            return String(value || "")
        }
    }

    function copyPlainText(text) {
        var value = String(text || "")
        if (value.length === 0)
            return
        if (bridge && typeof bridge.copyToClipboard === "function")
            bridge.copyToClipboard(value)
        else
            composeText.text = value
    }

    function copyStatisticsText() {
        if (!ft2Link)
            return
        var text = ft2Link.statisticsText()
        copyPlainText(text)
    }

    function exportLog(kind) {
        if (!ft2Link)
            return
        var mode = String(kind || "OPS").toUpperCase()
        if (mode === "ADIF")
            logExportText = ft2Link.adifLog()
        else if (mode === "OUTBOX" && typeof ft2Link.logbookOutboxText === "function")
            logExportText = ft2Link.logbookOutboxText()
        else if (mode === "CHAT")
            logExportText = ft2Link.chatHistoryLog()
        else if (mode === "CLUSTER" && typeof ft2Link.clusterExportJson === "function")
            logExportText = ft2Link.clusterExportJson()
        else if (mode === "STORE")
            logExportText = ft2Link.localStoreJson()
        else if (mode === "BUNDLE")
            logExportText = ft2Link.logsBundleText()
        else
            logExportText = ft2Link.operationalLog()
    }

    function exportCluster() {
        if (!ft2Link || typeof ft2Link.clusterExportJson !== "function")
            return
        var text = ft2Link.clusterExportJson()
        logExportText = text
        clusterJsonArea.text = text
        databaseActionText = "Cluster export " + String(root.clusterLastHeard.length) + " records"
    }

    function importCluster() {
        if (!ft2Link || typeof ft2Link.importClusterLastHeard !== "function")
            return
        var text = clusterJsonArea.text.trim()
        if (text.length === 0)
            text = logExportText.trim()
        if (text.length === 0)
            return
        var result = ft2Link.importClusterLastHeard(text, nowMs())
        databaseActionText = prettyJson(result)
        refreshClusterLastHeard()
        refreshStatistics()
        refreshStoreAudit()
    }

    function clusterAutoSyncIntervalText() {
        if (clusterAutoSyncSeconds < 60)
            return String(clusterAutoSyncSeconds) + "s"
        if (clusterAutoSyncSeconds % 60 === 0)
            return String(clusterAutoSyncSeconds / 60) + "m"
        return String(clusterAutoSyncSeconds) + "s"
    }

    function cycleClusterAutoSyncInterval() {
        var options = [30, 60, 120, 300, 600, 900]
        var index = 0
        for (var i = 0; i < options.length; ++i) {
            if (clusterAutoSyncSeconds <= options[i]) {
                index = i
                break
            }
        }
        clusterAutoSyncSeconds = options[(index + 1) % options.length]
    }

    function clusterShareSummary(prefix, result) {
        if (!result)
            return prefix + " no result"
        if (!result.ok)
            return prefix + " error: " + String(result.error || "unknown")
        var records = Number(result.records || result.total || root.clusterLastHeard.length || 0)
        var imported = Number(result.imported || 0)
        var merged = Number(result.merged || 0)
        var action = result.action ? String(result.action).toUpperCase() : prefix
        if (imported > 0 || merged > 0)
            return action + " +" + imported + " / ~" + merged + " / " + records + " rec"
        return action + " " + records + " rec"
    }

    function pushClusterShare() {
        if (!ft2Link || typeof ft2Link.writeClusterShareFile !== "function")
            return
        var result = ft2Link.writeClusterShareFile(clusterSharePath.trim())
        databaseActionText = prettyJson(result)
        clusterSyncStatus = clusterShareSummary("PUSH", result)
        if (result && result.ok) {
            clusterSharePath = String(result.path || clusterSharePath)
            clusterJsonArea.text = ft2Link.clusterExportJson()
        }
        refreshClusterLastHeard()
        refreshStatistics()
        refreshStoreAudit()
    }

    function pullClusterShare() {
        if (!ft2Link || typeof ft2Link.mergeClusterShareFile !== "function")
            return
        var result = ft2Link.mergeClusterShareFile(clusterSharePath.trim(), nowMs())
        databaseActionText = prettyJson(result)
        clusterSyncStatus = clusterShareSummary("PULL", result)
        if (result && result.ok)
            clusterSharePath = String(result.path || clusterSharePath)
        refreshClusterLastHeard()
        refreshStatistics()
        refreshStoreAudit()
    }

    function syncClusterShare(showDetail) {
        if (!ft2Link)
            return false
        var detail = showDetail === undefined ? true : !!showDetail
        var result = null
        if (typeof ft2Link.syncClusterShareFile === "function")
            result = ft2Link.syncClusterShareFile(clusterSharePath.trim(), nowMs())
        else if (typeof ft2Link.mergeClusterShareFile === "function"
                 && typeof ft2Link.writeClusterShareFile === "function") {
            var pull = ft2Link.mergeClusterShareFile(clusterSharePath.trim(), nowMs())
            if (pull && pull.ok)
                result = ft2Link.writeClusterShareFile(String(pull.path || clusterSharePath).trim())
            else if (pull && String(pull.error || "").indexOf("does not exist") >= 0)
                result = ft2Link.writeClusterShareFile(clusterSharePath.trim())
            else
                result = pull
        }
        if (!result)
            return false
        if (result.path)
            clusterSharePath = String(result.path)
        clusterSyncStatus = clusterShareSummary("SYNC", result)
        if (detail)
            databaseActionText = prettyJson(result)
        if (result.ok && typeof ft2Link.clusterExportJson === "function")
            clusterJsonArea.text = ft2Link.clusterExportJson()
        refreshClusterLastHeard()
        refreshStatistics()
        refreshStoreAudit()
        return !!result.ok
    }

    function clearCluster() {
        if (!ft2Link || typeof ft2Link.clearClusterLastHeard !== "function")
            return
        ft2Link.clearClusterLastHeard()
        refreshClusterLastHeard()
        refreshStatistics()
        refreshStoreAudit()
    }

    function writeAdifFile() {
        if (!ft2Link || typeof ft2Link.writeAdifLogFile !== "function")
            return
        var result = ft2Link.writeAdifLogFile("")
        logExportText = prettyJson(result)
        refreshStoreAudit()
    }

    function queueSelectedLogbookUpload() {
        if (!ft2Link || typeof ft2Link.queueLogbookUpload !== "function"
            || selectedSessionId === 0)
            return
        var result = ft2Link.queueLogbookUpload(selectedSessionId, "ALL", nowMs())
        logExportText = prettyJson(result)
        refreshLogbookOutbox()
        refreshStatistics()
        refreshStoreAudit()
    }

    function queueAllLogbookUploads() {
        if (!ft2Link || typeof ft2Link.queueAllLogbookUploads !== "function")
            return
        var result = ft2Link.queueAllLogbookUploads("ALL", nowMs())
        logExportText = prettyJson(result)
        refreshLogbookOutbox()
        refreshStatistics()
        refreshStoreAudit()
    }

    function sendQueuedLogbookUploads() {
        var hasOutboxUpload = bridge && typeof bridge.uploadExternalAdifForOutbox === "function"
        var hasLegacyUpload = bridge && typeof bridge.uploadExternalAdif === "function"
        if (!ft2Link || !bridge
            || typeof ft2Link.logbookUploadPayload !== "function"
            || typeof ft2Link.markLogbookUpload !== "function"
            || (!hasOutboxUpload && !hasLegacyUpload))
            return
        var sent = 0
        var failed = 0
        var skipped = 0
        var details = []
        for (var i = 0; i < logbookOutbox.length; ++i) {
            var row = logbookOutbox[i]
            var state = String(row.state || "")
            if (state !== "Queued" && state !== "Failed")
                continue
            var id = Number(row.id || 0)
            var payload = ft2Link.logbookUploadPayload(id)
            if (!payload || !payload.ok) {
                ++failed
                continue
            }
            var result = hasOutboxUpload
                         ? bridge.uploadExternalAdifForOutbox(id,
                                                              String(payload.remoteCall || ""),
                                                              String(payload.adif || ""),
                                                              String(payload.target || "ALL"))
                         : bridge.uploadExternalAdif(String(payload.remoteCall || ""),
                                                     String(payload.adif || ""),
                                                     String(payload.target || "ALL"))
            var ok = !!(result && result.ok)
            var nextState = ok ? String(result.state || "Submitted") : "Failed"
            var detail = result && result.detail ? String(result.detail)
                         : (result && result.error ? String(result.error) : "")
            ft2Link.markLogbookUpload(id, nextState, detail, nowMs())
            if (ok)
                ++sent
            else
                ++failed
            if (result && result.skipped)
                skipped += result.skipped.length
            details.push(result)
        }
        refreshLogbookOutbox()
        refreshStatistics()
        refreshStoreAudit()
        logExportText = prettyJson({
            ok: failed === 0 && sent > 0,
            submitted: sent,
            failed: failed,
            skipped: skipped,
            details: details
        })
    }

    function clearLogbookOutbox() {
        if (!ft2Link || typeof ft2Link.clearLogbookOutbox !== "function")
            return
        ft2Link.clearLogbookOutbox()
        refreshLogbookOutbox()
        refreshStatistics()
        refreshStoreAudit()
        logExportText = "Logbook outbox cleared"
    }

    function copyAdifPath() {
        if (!ft2Link || typeof ft2Link.adifLogPath !== "function")
            return
        copyPlainText(ft2Link.adifLogPath())
    }

    function backupStore() {
        if (!ft2Link)
            return
        var result = ft2Link.backupLocalStore("")
        databaseActionText = prettyJson(result)
        refreshStoreAudit()
    }

    function fixStore(makeBackup) {
        if (!ft2Link)
            return
        var result = ft2Link.fixLocalStore(!!makeBackup)
        databaseActionText = prettyJson(result)
        refreshStoreAudit()
    }

    function saveFrequencyPresets() {
        if (!ft2Link || typeof ft2Link.setFrequencyPresets !== "function")
            return
        var result = ft2Link.setFrequencyPresets(frequencyPresetText.text)
        databaseActionText = prettyJson(result)
        if (result && result.ok && result.text)
            frequencyPresetText.text = String(result.text)
        refreshFrequencyPlan()
        refreshStatistics()
        refreshStoreAudit()
        refreshQsyPlan()
    }

    function resetFrequencyPresets() {
        if (!ft2Link || typeof ft2Link.resetFrequencyPresets !== "function")
            return
        var result = ft2Link.resetFrequencyPresets()
        databaseActionText = prettyJson(result)
        if (result && result.text)
            frequencyPresetText.text = String(result.text)
        refreshFrequencyPlan()
        refreshStatistics()
        refreshStoreAudit()
    }

    function saveAllowedQsyRanges() {
        if (!ft2Link || typeof ft2Link.setAllowedQsyRanges !== "function")
            return
        var result = ft2Link.setAllowedQsyRanges(allowedQsyRangeText.text)
        databaseActionText = prettyJson(result)
        if (result && result.ok && result.text)
            allowedQsyRangeText.text = String(result.text)
        refreshFrequencyPlan()
        refreshStatistics()
        refreshStoreAudit()
        refreshQsyPlan()
    }

    function resetAllowedQsyRanges() {
        if (!ft2Link || typeof ft2Link.resetAllowedQsyRanges !== "function")
            return
        var result = ft2Link.resetAllowedQsyRanges()
        databaseActionText = prettyJson(result)
        if (result && result.text)
            allowedQsyRangeText.text = String(result.text)
        refreshFrequencyPlan()
        refreshStatistics()
        refreshStoreAudit()
        refreshQsyPlan()
    }

    function saveFrequencySchedule() {
        if (!ft2Link || typeof ft2Link.setFrequencySchedule !== "function")
            return
        var result = ft2Link.setFrequencySchedule(frequencyScheduleText.text)
        databaseActionText = prettyJson(result)
        if (result && result.ok)
            frequencyScheduleText.text = String(result.text || "")
        refreshFrequencyPlan()
        refreshStatistics()
        refreshStoreAudit()
    }

    function resetFrequencySchedule() {
        if (!ft2Link || typeof ft2Link.resetFrequencySchedule !== "function")
            return
        var result = ft2Link.resetFrequencySchedule()
        databaseActionText = prettyJson(result)
        frequencyScheduleText.text = ""
        refreshFrequencyPlan()
        refreshStatistics()
        refreshStoreAudit()
    }

    function auditStore() {
        refreshStoreAudit()
        databaseActionText = prettyJson(storeAudit)
    }

    function currentFormTemplate() {
        if (formTemplates.length === 0)
            return null
        return formTemplates[Math.max(0, Math.min(formTemplateIndex, formTemplates.length - 1))]
    }

    function currentCqType() {
        if (cqTypeOptions.length === 0)
            return "CQ"
        return String(cqTypeOptions[Math.max(0, Math.min(cqTypeIndex, cqTypeOptions.length - 1))])
    }

    function cycleCqType() {
        if (cqTypeOptions.length === 0)
            return
        cqTypeIndex = (cqTypeIndex + 1) % cqTypeOptions.length
    }

    function currentCqLocator() {
        var locator = cqLocator.trim().toUpperCase()
        if (locator.length === 0 && bridge && bridge.grid)
            locator = String(bridge.grid).trim().toUpperCase()
        locator = locator.replace(/[^A-Z0-9]/g, "")
        return locator.slice(0, 8)
    }

    function currentFormType() {
        var template = currentFormTemplate()
        return template ? String(template.id || "ICS213") : "ICS213"
    }

    function currentFormLabel() {
        var template = currentFormTemplate()
        return template ? String(template.label || template.id || "FORM") : "FORM"
    }

    function cycleFormTemplate() {
        if (formTemplates.length === 0)
            return
        formTemplateIndex = (formTemplateIndex + 1) % formTemplates.length
        applyCurrentFormTemplate()
    }

    function applyCurrentFormTemplate() {
        var template = currentFormTemplate()
        if (!template)
            return
        formFieldsText.text = String(template.fields || "").replace(/\n/g, "; ")
    }

    function parseFormFields(text) {
        var map = {}
        var parts = String(text || "").split(/[;\n]/)
        for (var i = 0; i < parts.length; ++i) {
            var line = parts[i].trim()
            if (line.length === 0)
                continue
            var at = line.indexOf("=")
            if (at <= 0) {
                map["message"] = line
                continue
            }
            var key = line.slice(0, at).trim()
            var value = line.slice(at + 1).trim()
            if (key.length > 0)
                map[key] = value
        }
        return map
    }

    function updateSelectedSessionFromSessions() {
        selectedRemoteCall = ""
        selectedSessionStateName = ""
        for (var i = 0; i < sessions.length; ++i) {
            if (Number(sessions[i].sessionId) === selectedSessionId) {
                selectedRemoteCall = String(sessions[i].remoteCall || "")
                selectedSessionStateName = String(sessions[i].stateName || "")
                break
            }
        }
    }

    function selectSession(sessionId) {
        selectedSessionId = Number(sessionId)
        updateSelectedSessionFromSessions()
        chatUnreadBelow = false
        chatScrollPinned = true
        refreshMessages()
        Qt.callLater(scrollChatToEnd)
    }

    function closeSelectedSession() {
        if (!ft2Link || selectedSessionId === 0)
            return false
        if (ft2Link.closeSession(selectedSessionId, nowMs())) {
            refreshSessions()
            return true
        }
        return false
    }

    function disconnectSelectedSession() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!selectedSessionConnected) {
            closeSelectedSession()
            return
        }
        var farewell = ft2Link.expandCannedMessage("73 <CALL> DE <MYCALL> <DISC>",
                                                   selectedSessionId,
                                                   nowMs())
        composeText.text = farewell
        if (sendChatText())
            closeSelectedSession()
    }

    function startSession(call) {
        if (!ft2Link || !call)
            return
        if (ft2Link.radioTxArmed) {
            startRadioSession(call)
            return
        }
        syncLocalStation()
        applyCapabilities()
        lastHelloBytes = ft2Link.startSessionHelloBytes(String(call), nowMs())
        selectedRemoteCall = String(call)
        selectedSessionId = ft2Link.activeSessionId
        refreshSessions()
    }

    function startRadioSession(call) {
        if (!ft2Link || !call)
            return
        syncLocalStation()
        applyCapabilities()
        lastHelloBytes = null
        if (ft2Link.startSessionRadioHandshake(String(call), nowMs())) {
            selectedRemoteCall = String(call)
            selectedSessionId = ft2Link.activeSessionId
            refreshSessions()
        }
    }

    function transmitBeacon(cq) {
        if (!ft2Link)
            return
        syncLocalStation()
        applyCapabilities()
        var type = currentCqType()
        var locator = currentCqLocator()
        if (cq && !skipCqSlot) {
            var slot = currentCqSlot()
            if (slot) {
                if (typeof ft2Link.transmitSpecialCqRadio === "function"
                    ? ft2Link.transmitSpecialCqRadio(type, locator, Number(slot.slotId || 0), 750, nowMs())
                    : ft2Link.transmitCqSlotRadio(Number(slot.slotId || 0), 750, nowMs()))
                    cqSlotWaitUntilMs = nowMs() + cqSlotWaitSeconds * 1000
                return
            }
        }
        if (cq && typeof ft2Link.transmitSpecialCqRadio === "function")
            ft2Link.transmitSpecialCqRadio(type, locator, 0, 750, nowMs())
        else
            ft2Link.transmitBeaconRadio(cq, nowMs())
    }

    function configureAutoBeacon(enabled) {
        if (!ft2Link)
            return
        syncLocalStation()
        applyCapabilities()
        if (!ft2Link.configureAutoBeacon(enabled, beaconIntervalSeconds, true, nowMs()))
            autoBeaconCheck.checked = false
    }

    function toggleAutoBeacon(enabled) {
        if (!ft2Link)
            return
        if (enabled && !ft2Link.radioTxArmed)
            ft2Link.setRadioTxArmed(true)
        configureAutoBeacon(enabled)
    }

    function cycleBeaconInterval() {
        if (beaconIntervalSeconds === 180)
            beaconIntervalSeconds = 300
        else if (beaconIntervalSeconds === 300)
            beaconIntervalSeconds = 600
        else
            beaconIntervalSeconds = 180
        if (ft2Link && ft2Link.autoBeaconEnabled)
            ft2Link.configureAutoBeacon(true, beaconIntervalSeconds, true, nowMs())
    }

    function beaconIntervalText() {
        return Math.round(beaconIntervalSeconds / 60) + " min"
    }

    function beaconCooldownSeconds() {
        if (!ft2Link || typeof ft2Link.beaconCooldownSeconds !== "function")
            return 0
        return Math.max(0, Number(ft2Link.beaconCooldownSeconds(nowMs())))
    }

    function clampValue(value, minValue, maxValue) {
        var number = Math.round(Number(value))
        if (!isFinite(number))
            number = minValue
        return Math.max(minValue, Math.min(maxValue, number))
    }

    function resizeStationPane(width) {
        stationPaneWidth = clampValue(width, 160, 420)
    }

    function resizeSessionPane(width) {
        sessionPaneWidth = clampValue(width, 130, 340)
    }

    function messageListAtEnd() {
        if (typeof messageList === "undefined" || !messageList)
            return true
        if (messageList.contentHeight <= messageList.height + 4)
            return true
        return messageList.atYEnd
               || messageList.contentY >= messageList.contentHeight - messageList.height - 10
    }

    function scrollChatToEnd() {
        if (typeof messageList === "undefined" || !messageList)
            return
        messageList.positionViewAtEnd()
        chatUnreadBelow = false
        chatScrollPinned = true
    }

    function cqTxButtonText() {
        return "CQ TX"
    }

    function armOrTransmitBeacon(cq) {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        transmitBeacon(cq)
    }

    function armOrTransmitBroadcast() {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendBroadcastText()
    }

    function armOrTransmitPathFinderRequest() {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendPathFinderRequest()
    }

    function armOrTransmitPathFinderResponse() {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendPathFinderResponse()
    }

    function armOrTransmitMailbox() {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendMailboxText()
    }

    function armOrTransmitRelayMailbox() {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendRelayMailbox()
    }

    function armOrTransmitForm() {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendFormText()
    }

    function armOrTransmitFile() {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendFileText()
    }

    function armOrTransmitBulletin() {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendBulletinText()
    }

    function armOrTransmitPing(call) {
        if (!ft2Link)
            return
        if (!ft2Link.radioTxArmed) {
            ft2Link.setRadioTxArmed(true)
            return
        }
        sendPing(call)
    }

    function completeLoopbackAck() {
        if (!ft2Link || !lastHelloBytes || selectedRemoteCall.length === 0)
            return
        var ack = ft2Link.answerHelloBytes(selectedRemoteCall, lastHelloBytes, nowMs())
        if (ack && ack.length > 0)
            ft2Link.receiveHelloAckBytes(ack, nowMs())
        selectedSessionId = ft2Link.activeSessionId
        refreshSessions()
    }

    function addManualStation() {
        if (!ft2Link)
            return
        var call = manualCall.text.trim().toUpperCase()
        if (call.length === 0)
            return
        applyCapabilities()
        ft2Link.observeStation(call, manualGrid.text.trim().toUpperCase(), manualName.text.trim(),
                               manualCq.checked, true, true, true, true,
                               preferW2300 ? 2 : 1,
                               robustMode ? 1 : 0,
                               nowMs())
        if (manualTag.text.trim().length > 0)
            ft2Link.setContactTag(call, manualTag.text.trim(), nowMs())
        manualCall.text = ""
        manualTag.text = ""
        refreshStations()
        refreshContactHistory()
    }

    function sendChatText() {
        if (!ft2Link || selectedSessionId === 0)
            return false
        var text = composeText.text.trim()
        if (text.length === 0)
            return false
        if (ft2Link.transmitTextLocalAudio(selectedSessionId, text, nowMs(),
                                           ackAudioCheck.checked,
                                           dropDataCheck.checked,
                                           dropAckCheck.checked)) {
            composeText.text = ""
            refreshSessions()
            return true
        }
        refreshSessions()
        return false
    }

    function sendBroadcastText() {
        if (!ft2Link)
            return
        var text = broadcastText.text.trim()
        if (text.length === 0)
            return
        if (ft2Link.transmitBroadcastRadio(text, nowMs())) {
            broadcastText.text = ""
            refreshBroadcasts()
            refreshAlerts()
        }
    }

    function saveAlertTags() {
        if (!ft2Link || typeof ft2Link.setCustomAlertTags !== "function")
            return
        var result = ft2Link.setCustomAlertTags(alertTagsText.text)
        databaseActionText = prettyJson(result)
        refreshAlertTags()
        refreshStatistics()
        refreshStoreAudit()
    }

    function clearAlertTags() {
        if (!ft2Link || typeof ft2Link.clearCustomAlertTags !== "function")
            return
        var result = ft2Link.clearCustomAlertTags()
        databaseActionText = prettyJson(result)
        alertTagsText.text = ""
        refreshAlertTags()
        refreshStatistics()
        refreshStoreAudit()
    }

    function pathFinderTarget() {
        return pathTargetText.text.trim().toUpperCase()
    }

    function pathFinderCandidate() {
        if (!ft2Link)
            return null
        var target = pathFinderTarget()
        if (target.length === 0)
            return null
        var item = ft2Link.pathFinderCandidate(target, nowMs())
        if (!item || !item.canRespond)
            return null
        return item
    }

    function pathRelayHintForTarget(target) {
        if (!ft2Link || typeof ft2Link.pathRelayCandidate !== "function")
            return null
        var call = String(target || "").trim().toUpperCase()
        if (call.length === 0)
            return null
        var item = ft2Link.pathRelayCandidate(call, nowMs())
        if (!item || !item.canRelay)
            return null
        return item
    }

    function pathRelayHint() {
        var target = ""
        if (typeof mailToText !== "undefined" && mailToText)
            target = mailToText.text.trim().toUpperCase()
        if (target.length === 0)
            target = pathFinderTarget()
        return pathRelayHintForTarget(target)
    }

    function pathRelayLine() {
        var item = pathRelayHint()
        if (!item)
            return "No path relay hint"
        if (String(item.line || "").length > 0)
            return String(item.line)
        var target = String(item.targetCall || "--")
        var relay = String(item.relayCall || "--")
        var parked = Number(item.parkedMailboxCount || 0)
        var heardAge = Number(item.heardAgeMinutes)
        var heard = isFinite(heardAge) && heardAge >= 0 ? " heard " + heardAge + "m" : ""
        return "Relay " + relay + " -> " + target + heard
               + " / mail " + parked
               + (parked > 0 ? " / ready" : " / park mail first")
    }

    function usePathRelayForMail() {
        var item = pathRelayHint()
        if (!item)
            return
        mailToText.text = String(item.targetCall || "")
        if (String(item.mailboxSubject || "").length > 0
            && mailSubjectText.text.trim().length === 0)
            mailSubjectText.text = String(item.mailboxSubject)
        toolPageIndex = 5
    }

    function callPathRelay() {
        var item = pathRelayHint()
        if (!item)
            return
        startSession(String(item.relayCall || ""))
    }

    function forwardPathRelay() {
        var item = pathRelayHint()
        if (!item || !item.readyToForward)
            return
        usePathRelayForMail()
        startSession(String(item.relayCall || ""))
    }

    function stationRelayWorkflow(station) {
        if (!station)
            return null
        var workflow = station.relayWorkflow
        if (workflow && workflow.canRelay)
            return workflow
        if (!ft2Link || typeof ft2Link.relayWorkflowForStation !== "function")
            return null
        var call = String(station.call || "").trim().toUpperCase()
        if (call.length === 0)
            return null
        workflow = ft2Link.relayWorkflowForStation(call, nowMs())
        return workflow && workflow.canRelay ? workflow : null
    }

    function stationSubtitle(station) {
        var workflow = stationRelayWorkflow(station)
        if (workflow && String(workflow.line || "").length > 0)
            return String(workflow.line)
        return (station.cqLocator || station.locator || "--")
               + "  " + (station.name || "")
               + (root.stationHistoryMode && String(station.profileName || "").length > 0
                  ? "  " + String(station.profileName || "")
                  : "")
               + (!root.stationHistoryMode && Number(station.parkedMailboxCount || 0) > 0
                  ? "  MAIL " + Number(station.parkedMailboxCount || 0)
                  : "")
               + (!root.stationHistoryMode && String(station.pathRelayTarget || "").length > 0
                  ? "  RLY>" + String(station.pathRelayTarget || "")
                  : "")
    }

    function stationSubtitleColor(station) {
        var workflow = stationRelayWorkflow(station)
        if (workflow) {
            var priority = String(workflow.priority || "NORMAL")
            if (priority === "EMCOMM")
                return root.red
            if (priority === "URGENT")
                return root.amber
            return root.green
        }
        return String(station.pathRelayTarget || "").length > 0
               ? root.amber
               : (Number(station.parkedMailboxCount || 0) > 0
                  ? root.green
                  : root.textSecondary)
    }

    function useStationRelayWorkflow(station) {
        var workflow = stationRelayWorkflow(station)
        if (!workflow)
            return
        mailToText.text = String(workflow.targetCall || "")
        if (String(workflow.mailboxSubject || "").length > 0)
            mailSubjectText.text = String(workflow.mailboxSubject || "")
        pathTargetText.text = String(workflow.targetCall || "")
        toolPageIndex = 5
        startSession(String(workflow.relayCall || station.call || ""))
    }

    function sendPathFinderRequest() {
        if (!ft2Link)
            return
        var target = pathFinderTarget()
        if (target.length === 0)
            return
        if (ft2Link.transmitPathFinderRadio(target, nowMs())) {
            refreshBroadcasts()
            refreshAlerts()
        }
    }

    function sendPathFinderResponse() {
        if (!ft2Link)
            return
        var target = pathFinderTarget()
        if (target.length === 0)
            return
        if (ft2Link.transmitPathFinderResponseRadio(target, nowMs())) {
            refreshBroadcasts()
            refreshAlerts()
        }
    }

    function sendMailboxText() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!guardWideTx("MAIL"))
            return
        var body = mailBodyText.text.trim()
        if (body.length === 0)
            return
        var toCall = mailToText.text.trim().toUpperCase()
        if (toCall.length === 0)
            toCall = selectedRemoteCall
        var ok = typeof ft2Link.transmitMailboxRadioTyped === "function"
                 ? ft2Link.transmitMailboxRadioTyped(selectedSessionId,
                                                     toCall,
                                                     mailSubjectText.text.trim(),
                                                     body,
                                                     mailUrgentCheck.checked,
                                                     mailEmcommCheck.checked,
                                                     nowMs())
                 : ft2Link.transmitMailboxRadio(selectedSessionId,
                                                toCall,
                                                mailSubjectText.text.trim(),
                                                body,
                                                nowMs())
        if (ok) {
            mailBodyText.text = ""
            refreshMailbox()
            refreshSessions()
        }
    }

    function parkMailboxText() {
        if (!ft2Link)
            return
        var body = mailBodyText.text.trim()
        var toCall = mailToText.text.trim().toUpperCase()
        if (body.length === 0 || toCall.length === 0)
            return
        var ok = typeof ft2Link.parkMailboxTyped === "function"
                 ? ft2Link.parkMailboxTyped(toCall,
                                            mailSubjectText.text.trim(),
                                            body,
                                            mailUrgentCheck.checked,
                                            mailEmcommCheck.checked,
                                            nowMs())
                 : ft2Link.parkMailbox(toCall,
                                       mailSubjectText.text.trim(),
                                       body,
                                       nowMs())
        if (ok) {
            mailBodyText.text = ""
            refreshMailbox()
            refreshAlerts()
        }
    }

    function copyMailboxText() {
        if (!ft2Link || typeof ft2Link.mailboxText !== "function")
            return
        copyPlainText(ft2Link.mailboxText())
    }

    function copyRelayQueueText() {
        if (!ft2Link || typeof ft2Link.relayQueueText !== "function")
            return
        copyPlainText(ft2Link.relayQueueText(nowMs()))
    }

    function relayMailboxCandidate() {
        if (!ft2Link || selectedSessionId === 0)
            return null
        var item = ft2Link.relayMailboxForSession(selectedSessionId)
        if (!item || Number(item.id || 0) === 0)
            return null
        return item
    }

    function relayMailboxButtonText() {
        var item = relayMailboxCandidate()
        if (!item)
            return "RELAY"
        return ft2Link && ft2Link.radioTxArmed ? "RELAY TX" : "ARM R"
    }

    function sendRelayMailbox() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!guardWideTx("RELAY"))
            return
        var item = relayMailboxCandidate()
        if (!item)
            return
        if (ft2Link.transmitRelayMailboxRadio(selectedSessionId,
                                              Number(item.id),
                                              nowMs())) {
            refreshMailbox()
            refreshSessions()
        }
    }

    function markMailboxItemRead(item, read) {
        if (!ft2Link || !item)
            return
        if (ft2Link.markMailboxRead(Number(item.id || 0), read, nowMs()))
            refreshMailbox()
    }

    function deleteMailboxItem(item) {
        if (!ft2Link || !item)
            return
        if (ft2Link.deleteMailboxMessage(Number(item.id || 0)))
            refreshMailbox()
    }

    function mailboxEmailDraft(item) {
        if (!ft2Link || !item || typeof ft2Link.mailboxEmailGateway !== "function")
            return null
        return ft2Link.mailboxEmailGateway(Number(item.id || 0), "")
    }

    function openMailboxEmail(item) {
        var draft = mailboxEmailDraft(item)
        if (!draft || !draft.ok)
            return
        logExportText = draft.eml || prettyJson(draft)
        if (draft.mailtoReady && bridge && typeof bridge.openExternalUrl === "function") {
            if (bridge.openExternalUrl(String(draft.mailtoUrl || "")))
                return
        }
        copyPlainText(String(draft.eml || ""))
    }

    function saveMailboxEml(item) {
        var draft = mailboxEmailDraft(item)
        if (!draft || !draft.ok)
            return
        var fileName = String(draft.emlFileName || "FT2-Link_VMail.eml")
        var path = ""
        if (bridge && typeof bridge.saveFileDialog === "function")
            path = bridge.saveFileDialog("Save FT2-Link VMail email", fileName,
                                         ["Email message (*.eml)", "All files (*)"])
        if (path.length === 0) {
            copyPlainText(String(draft.eml || ""))
            logExportText = String(draft.eml || "")
            return
        }
        if (bridge && typeof bridge.writeTextFile === "function") {
            var result = bridge.writeTextFile(path, String(draft.eml || ""))
            logExportText = prettyJson(result)
        } else {
            copyPlainText(String(draft.eml || ""))
            logExportText = String(draft.eml || "")
        }
    }

    function emailGatewaySecurity() {
        var index = Math.max(0, Math.min(emailGatewaySecurityIndex,
                                         emailGatewaySecurityOptions.length - 1))
        return String(emailGatewaySecurityOptions[index] || "STARTTLS")
    }

    function emailGatewayConfig(draft) {
        return {
            host: emailGatewayHost.trim(),
            port: emailGatewayPort,
            security: emailGatewaySecurity(),
            username: emailGatewayUsername.trim(),
            fromEmail: emailGatewayFrom.trim().length > 0
                       ? emailGatewayFrom.trim()
                       : profileEmail.trim(),
            toEmail: draft ? String(draft.toEmail || "") : "",
            auth: emailGatewayUsername.trim().length > 0
        }
    }

    function cycleEmailGatewaySecurity() {
        emailGatewaySecurityIndex = (emailGatewaySecurityIndex + 1)
                                    % emailGatewaySecurityOptions.length
    }

    function setEmailGatewayItemState(mailboxId, state, detail) {
        var key = String(Number(mailboxId || 0))
        var next = {}
        for (var existing in emailGatewayRequestStates)
            next[existing] = emailGatewayRequestStates[existing]
        next[key] = {
            state: String(state || ""),
            detail: String(detail || "")
        }
        emailGatewayRequestStates = next
    }

    function emailGatewayItemState(item) {
        if (!item)
            return ""
        var entry = emailGatewayRequestStates[String(Number(item.id || 0))]
        if (!entry)
            return String(item.emailGatewayState || "")
        return String(entry.state || "")
    }

    function saveEmailGatewayPassword() {
        if (!bridge || typeof bridge.setFt2LinkEmailGatewayPassword !== "function"
                || typeof emailGatewayPasswordText === "undefined")
            return
        var password = emailGatewayPasswordText.text
        if (password.length === 0) {
            emailGatewayStatus = "SMTP password empty"
            return
        }
        var result = bridge.setFt2LinkEmailGatewayPassword(emailGatewayConfig(null), password)
        emailGatewayPasswordText.text = ""
        emailGatewayStatus = result && result.ok ? "SMTP password saved securely"
                                                 : "SMTP password error: " + String(result ? result.error || "unknown" : "unknown")
        logExportText = prettyJson(result)
    }

    function clearEmailGatewayPassword() {
        if (!bridge || typeof bridge.clearFt2LinkEmailGatewayPassword !== "function")
            return
        var result = bridge.clearFt2LinkEmailGatewayPassword(emailGatewayConfig(null))
        emailGatewayStatus = result && result.ok ? "SMTP password cleared"
                                                 : "SMTP password error: " + String(result ? result.error || "unknown" : "unknown")
        logExportText = prettyJson(result)
    }

    function testEmailGateway() {
        if (!bridge || typeof bridge.testFt2LinkEmailGateway !== "function") {
            emailGatewayStatus = "SMTP test unavailable"
            return
        }
        var result = bridge.testFt2LinkEmailGateway(emailGatewayConfig(null))
        logExportText = prettyJson(result)
        if (result && result.ok)
            emailGatewayStatus = String(result.detail || "SMTP test queued")
        else
            emailGatewayStatus = "SMTP test error: " + String(result ? result.error || "unknown" : "unknown")
    }

    function sendMailboxGatewayEmail(item) {
        var draft = mailboxEmailDraft(item)
        if (!draft || !draft.ok) {
            emailGatewayStatus = "SMTP draft unavailable"
            return
        }
        if (!emailGatewayEnabled) {
            emailGatewayStatus = "SMTP gateway disabled"
            return
        }
        if (!draft.mailtoReady) {
            emailGatewayStatus = "SMTP recipient email missing"
            logExportText = draft.eml || prettyJson(draft)
            return
        }
        if (!bridge || typeof bridge.sendFt2LinkGatewayEmail !== "function") {
            emailGatewayStatus = "SMTP bridge unavailable"
            return
        }
        var config = emailGatewayConfig(draft)
        var result = bridge.sendFt2LinkGatewayEmail(Number(item.id || 0),
                                                    config,
                                                    String(draft.eml || ""))
        logExportText = prettyJson(result)
        if (result && result.ok) {
            emailGatewayStatus = String(result.detail || "SMTP queued")
            setEmailGatewayItemState(item.id, "Queued", emailGatewayStatus)
        } else {
            emailGatewayStatus = "SMTP error: " + String(result ? result.error || "unknown" : "unknown")
            setEmailGatewayItemState(item.id, "Failed", emailGatewayStatus)
        }
    }

    function loadContactDetails(item) {
        if (!item)
            return
        selectedContactCall = String(item.call || "").trim().toUpperCase()
        contactCallText.text = selectedContactCall
        contactGridText.text = String(item.locator || "")
        contactNameText.text = String(item.name || "")
        contactTagText.text = String(item.tag || "")
        contactCommentText.text = String(item.comment || "")
        refreshContactTimeline()
    }

    function saveContactDetails() {
        if (!ft2Link)
            return
        var call = contactCallText.text.trim().toUpperCase()
        if (call.length === 0)
            return
        if (ft2Link.setContactDetails(call,
                                      contactGridText.text.trim(),
                                      contactNameText.text.trim(),
                                      contactTagText.text.trim(),
                                      contactCommentText.text.trim(),
                                      nowMs())) {
            selectedContactCall = call
            refreshContactHistory()
            refreshContactTimeline()
            refreshStations()
        }
    }

    function clearContactDetailsEditor() {
        selectedContactCall = ""
        contactCallText.text = ""
        contactGridText.text = ""
        contactNameText.text = ""
        contactTagText.text = ""
        contactCommentText.text = ""
        selectedContactTimeline = []
    }

    function sendFormText() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!guardWideTx("FORM"))
            return
        var fields = parseFormFields(formFieldsText.text)
        if (Object.keys(fields).length === 0)
            return
        var toCall = formToText.text.trim().toUpperCase()
        if (toCall.length === 0)
            toCall = selectedRemoteCall
        if (ft2Link.transmitFormRadio(selectedSessionId,
                                      toCall,
                                      currentFormType(),
                                      fields,
                                      nowMs())) {
            refreshForms()
            refreshSessions()
        }
    }

    function sendFileText() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!guardWideTx("FILE"))
            return
        var content = fileContentText.text.trim()
        if (content.length === 0)
            return
        var toCall = fileToText.text.trim().toUpperCase()
        if (toCall.length === 0)
            toCall = selectedRemoteCall
        if (ft2Link.transmitFileRadio(selectedSessionId,
                                      toCall,
                                      fileNameText.text.trim(),
                                      content,
                                      nowMs())) {
            fileContentText.text = ""
            refreshFileTransfers()
            refreshSessions()
        }
    }

    function sendBulletinText() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!guardWideTx("BBS"))
            return
        var body = bulletinBodyText.text.trim()
        if (body.length === 0)
            return
        if (ft2Link.transmitBulletinRadio(selectedSessionId,
                                          bulletinGroupText.text.trim(),
                                          bulletinTitleText.text.trim(),
                                          body,
                                          nowMs())) {
            bulletinBodyText.text = ""
            refreshBulletins()
            refreshSessions()
        }
    }

    function insertQslCard() {
        if (!ft2Link || selectedSessionId === 0)
            return
        var text = ft2Link.qslCard(selectedSessionId)
        if (String(text || "").length > 0)
            composeText.text = text
    }

    function copyAdifRecord() {
        if (!ft2Link || selectedSessionId === 0)
            return
        var text = ft2Link.adifRecord(selectedSessionId)
        if (String(text || "").length === 0)
            return
        if (bridge && typeof bridge.copyToClipboard === "function")
            bridge.copyToClipboard(text)
        else
            composeText.text = text
    }

    function sendPing(call) {
        if (!ft2Link)
            return
        var target = String(call || selectedRemoteCall || "").trim().toUpperCase()
        if (target.length === 0)
            return
        if (ft2Link.transmitPingRadio(target, nowMs()))
            refreshPingLog()
    }

    function insertCannedMessage(templateText) {
        if (!ft2Link)
            return
        var text = ft2Link.expandCannedMessage(String(templateText || ""),
                                               selectedSessionId,
                                               nowMs())
        if (text.length === 0)
            return
        composeText.text = text
        composeText.forceActiveFocus()
        composeText.cursorPosition = composeText.text.length
    }

    function loadPreset(item) {
        if (!item)
            return
        presetLabelText.text = String(item.label || "")
        presetTemplateText.text = String(item.templateText || "")
        presetTipText.text = String(item.tip || "")
    }

    function savePreset() {
        if (!ft2Link || typeof ft2Link.addOrUpdateCannedMessage !== "function")
            return
        var result = ft2Link.addOrUpdateCannedMessage(presetLabelText.text,
                                                      presetTemplateText.text,
                                                      presetTipText.text)
        databaseActionText = prettyJson(result)
        refreshCannedMessages()
        refreshStoreAudit()
    }

    function deletePreset() {
        if (!ft2Link || typeof ft2Link.deleteCannedMessage !== "function")
            return
        var result = ft2Link.deleteCannedMessage(presetLabelText.text)
        databaseActionText = prettyJson(result)
        if (result && result.ok) {
            presetLabelText.text = ""
            presetTemplateText.text = ""
            presetTipText.text = ""
        }
        refreshCannedMessages()
        refreshStoreAudit()
    }

    function resetPresets() {
        if (!ft2Link || typeof ft2Link.resetCannedMessages !== "function")
            return
        var result = ft2Link.resetCannedMessages()
        databaseActionText = prettyJson(result)
        presetLabelText.text = ""
        presetTemplateText.text = ""
        presetTipText.text = ""
        refreshCannedMessages()
        refreshStoreAudit()
    }

    function checkInBody() {
        if (!ft2Link || typeof ft2Link.checkInMessage !== "function")
            return ""
        return ft2Link.checkInMessage(checkInCityText.text,
                                      checkInRegionText.text,
                                      checkInChannelText.text,
                                      checkInWeatherText.text,
                                      nowMs())
    }

    function prepareCheckInMail() {
        var body = checkInBody()
        if (body.length === 0)
            return
        checkInCity = checkInCityText.text.trim()
        checkInRegion = checkInRegionText.text.trim()
        checkInChannel = checkInChannelText.text.trim()
        mailToText.text = "varacwednesday@gmail.com"
        mailSubjectText.text = "VarAC Wednesday Check-In"
        mailBodyText.text = body
        toolPageIndex = 5
    }

    function prepareCheckInChat() {
        var body = checkInBody()
        if (body.length === 0)
            return
        checkInCity = checkInCityText.text.trim()
        checkInRegion = checkInRegionText.text.trim()
        checkInChannel = checkInChannelText.text.trim()
        composeText.text = body
        composeText.forceActiveFocus()
        composeText.cursorPosition = composeText.text.length
        toolPageIndex = 0
    }

    function currentQsySlot() {
        if (qsySlots.length === 0)
            return null
        return qsySlots[Math.max(0, Math.min(qsySlotIndex, qsySlots.length - 1))]
    }

    function currentCqSlot() {
        if (qsySlots.length === 0)
            return null
        return qsySlots[Math.max(0, Math.min(cqSlotIndex, qsySlots.length - 1))]
    }

    function slotIdLabel(slotId) {
        var id = Number(slotId || 0)
        if (id > 0)
            return "S+" + id
        if (id < 0)
            return "S" + id
        return "S0"
    }

    function currentQsySlotLabel() {
        var slot = currentQsySlot()
        return slot ? String(slot.label || "QSY") : "QSY"
    }

    function currentCqSlotLabel() {
        var slot = currentCqSlot()
        return slot ? slotIdLabel(slot.slotId) : "SLOT"
    }

    function currentCqSlotOffsetLabel() {
        var slot = currentCqSlot()
        if (!slot)
            return "--"
        var offset = Number(slot.offsetHz || 0)
        return (offset > 0 ? "+" : "") + offset + " Hz"
    }

    function currentQsySlotTip() {
        var slot = currentQsySlot()
        return slot ? String(slot.tip || "Insert QSY invitation") : "Insert QSY invitation"
    }

    function currentCqSlotTip() {
        var slot = currentCqSlot()
        if (!slot)
            return "Select CQ slot"
        return "CQ will advertise " + slotIdLabel(slot.slotId) + " (" + currentCqSlotOffsetLabel() + ")"
    }

    function cycleQsySlot() {
        if (qsySlots.length === 0)
            return
        qsySlotIndex = (qsySlotIndex + 1) % qsySlots.length
    }

    function cycleCqSlot() {
        if (qsySlots.length === 0)
            return
        cqSlotIndex = (cqSlotIndex + 1) % qsySlots.length
    }

    function cycleCqSlotWait() {
        if (cqSlotWaitSeconds < 120)
            cqSlotWaitSeconds = 120
        else if (cqSlotWaitSeconds < 300)
            cqSlotWaitSeconds = 300
        else if (cqSlotWaitSeconds < 600)
            cqSlotWaitSeconds = 600
        else if (cqSlotWaitSeconds < 900)
            cqSlotWaitSeconds = 900
        else if (cqSlotWaitSeconds < 1800)
            cqSlotWaitSeconds = 1800
        else if (cqSlotWaitSeconds < 3600)
            cqSlotWaitSeconds = 3600
        else
            cqSlotWaitSeconds = 60
    }

    function cqSlotWaitText() {
        if (cqSlotWaitSeconds < 60)
            return cqSlotWaitSeconds + "s"
        if (cqSlotWaitSeconds % 60 === 0)
            return Math.round(cqSlotWaitSeconds / 60) + "m"
        return cqSlotWaitSeconds + "s"
    }

    function cqSlotWaitRemainingSeconds() {
        if (cqSlotWaitUntilMs <= nowMs())
            return 0
        return Math.ceil((cqSlotWaitUntilMs - nowMs()) / 1000)
    }

    function cqSlotBusy() {
        var slot = currentCqSlot()
        if (!slot)
            return false
        var slotId = Number(slot.slotId || 0)
        for (var i = 0; i < stations.length; ++i) {
            if (Number(stations[i].cqSlotId || 0) === slotId)
                return true
        }
        return false
    }

    function cqSlotStatusText() {
        var remaining = cqSlotWaitRemainingSeconds()
        if (remaining > 0)
            return "WAIT " + remaining + "s"
        return cqSlotBusy() ? "BUSY" : "FREE"
    }

    function insertQsySlotTag() {
        var slot = currentQsySlot()
        if (!slot)
            return
        root.insertCannedMessage(String(slot.tag || ""))
    }

    function currentDialFrequencyHz() {
        var hz = 0
        if (typeof bridge !== "undefined" && bridge) {
            if (bridge.dialFrequency !== undefined)
                hz = Number(bridge.dialFrequency)
            if ((!hz || hz <= 0) && bridge.frequency !== undefined)
                hz = Number(bridge.frequency)
        }
        if (!isFinite(hz) || hz <= 0)
            return 0
        return Math.round(hz)
    }

    function frequencyHzText(hz) {
        var value = Math.round(Number(hz || 0))
        if (!isFinite(value) || value <= 0)
            return "--"
        return (value / 1000000).toFixed(6) + " MHz"
    }

    function refreshQsyPlan() {
        qsyPlan = ({})
        if (!ft2Link || selectedMessages.length === 0
                || typeof ft2Link.qsyPlanForText !== "function")
            return
        var dial = currentDialFrequencyHz()
        for (var i = selectedMessages.length - 1; i >= 0; --i) {
            var message = selectedMessages[i]
            if (String(message.directionName || "") === "Outgoing")
                continue
            var plan = ft2Link.qsyPlanForText(String(message.text || ""), dial)
            if (plan && plan.valid) {
                qsyPlan = plan
                return
            }
        }
    }

    function qsyPlanValid() {
        return qsyPlan && qsyPlan.valid === true
    }

    function qsyPlanText() {
        if (!qsyPlanValid())
            return "No QSY invite"
        var text = String(qsyPlan.summary || "QSY invite")
        if (qsyPlan.hasTargetFrequency)
            text += "  " + frequencyHzText(qsyPlan.dialFrequencyHz)
        if (qsyPlan.rangeChecked)
            text += "  " + String(qsyPlan.rangeStatus || "")
        return text
    }

    function qsyCallingFrequencyText() {
        return qsyCallingFrequencyHz > 0 ? frequencyHzText(qsyCallingFrequencyHz) : "CF --"
    }

    function setCallingFrequencyFromRig() {
        var dial = currentDialFrequencyHz()
        if (dial > 0)
            qsyCallingFrequencyHz = dial
    }

    function callingFrequencyGuard(action) {
        if (!ft2Link || typeof ft2Link.callingFrequencyGuard !== "function")
            return ({ allowed: true, blocked: false })
        return ft2Link.callingFrequencyGuard(String(action || ""),
                                             currentDialFrequencyHz(),
                                             qsyCallingFrequencyHz)
    }

    function guardWideTx(action) {
        var result = callingFrequencyGuard(action)
        if (result && result.blocked) {
            databaseActionText = String(result.message || "Calling frequency guard")
            return false
        }
        return true
    }

    function insertCallingFrequencyQsyTag() {
        if (!ft2Link || qsyCallingFrequencyHz <= 0)
            return
        var tag = ft2Link.qsyFrequencyTag(qsyCallingFrequencyHz)
        if (String(tag || "").length > 0)
            insertCannedMessage(tag)
    }

    function sendQsyControlTag(tag) {
        if (!tag || !selectedSessionConnected)
            return
        composeText.text = String(tag)
        sendChatText()
    }

    function prepareRadioTx() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!guardWideTx("RF TX"))
            return
        var text = composeText.text.trim()
        if (text.length === 0)
            return
        ft2Link.prepareRadioTxAudio(selectedSessionId, text, nowMs())
    }

    function transmitRadioTx() {
        if (!ft2Link || selectedSessionId === 0)
            return
        if (!guardWideTx("RF TX"))
            return
        var text = composeText.text.trim()
        if (text.length === 0)
            return
        if (ft2Link.transmitPreparedRadioTxAudio(selectedSessionId, text, nowMs())) {
            composeText.text = ""
            refreshSessions()
        }
    }

    function metric(key, fallback) {
        if (!ft2Link || !ft2Link.lastTransportMetrics)
            return fallback
        var value = ft2Link.lastTransportMetrics[key]
        return value === undefined || value === null ? fallback : value
    }

    function fixedMetric(key, digits, fallback) {
        var value = Number(metric(key, NaN))
        return isFinite(value) ? value.toFixed(digits) : fallback
    }

    function radioMetric(key, fallback) {
        if (!ft2Link || !ft2Link.lastRadioTxPlan)
            return fallback
        var value = ft2Link.lastRadioTxPlan[key]
        return value === undefined || value === null ? fallback : value
    }

    function fixedRadioMetric(key, digits, fallback) {
        var value = Number(radioMetric(key, NaN))
        return isFinite(value) ? value.toFixed(digits) : fallback
    }

    function transportLine() {
        if (metric("liveRx", false))
            return "RX " + String(metric("profileName", "--"))
                   + "  " + String(metric("w2300RateModeName", "--"))
                   + "  q " + fixedMetric("quality", 2, "--")
                   + "  off " + fixedMetric("estimatedFrequencyOffsetHz", 1, "--") + " Hz"
                   + "  next " + String(metric("nextW2300RateModeName", "--"))
        return "RF " + String(metric("profileName", "--"))
               + "  " + fixedMetric("effectivePayloadBps", 1, "0.0") + " B/s"
               + "  " + metric("burstCount", 0) + " burst"
               + "  retry " + metric("retryBurstCount", 0)
               + "  drop " + (Number(metric("droppedBurstCount", 0)) + Number(metric("droppedAckBurstCount", 0)))
    }

    function radioLine() {
        var lbt = ft2Link && ft2Link.liveChannelBusy ? "  LBT BUSY" : ""
        var guard = callingFrequencyGuard("RF TX")
        var cf = guard && guard.blocked ? "  CF GUARD" : ""
        return "RADIO " + String(radioMetric("profileName", "--"))
               + "  " + String(radioMetric("w2300RateModeName", "--"))
               + "  " + String(radioMetric("w2300RateSource", "--"))
               + "  " + fixedRadioMetric("sampleRate", 0, "0") + " Hz"
               + "  " + fixedRadioMetric("audioSeconds", 2, "0.00") + " s"
               + "  " + radioMetric("burstCount", 0) + " burst"
               + lbt + cf
    }

    Component.onCompleted: {
        syncLocalStation()
        applyCapabilities()
        refreshFormTemplates()
        refreshStations()
        refreshSessions()
        refreshBroadcasts()
        refreshAlerts()
        refreshAlertTags()
        refreshMailbox()
        refreshForms()
        refreshFileTransfers()
        refreshReceivedFiles()
        refreshBulletins()
        refreshQsoLog()
        refreshLogbookOutbox()
        refreshContactHistory()
        refreshPingLog()
        refreshPathReports()
        refreshBeaconHistory()
        updateClusterFromRig()
        refreshPathAnalysis()
        refreshStatistics()
        refreshStoreAudit()
        refreshCannedMessages()
        refreshQsySlots()
        refreshFrequencyPlan()
        refreshPresence()
        refreshQsoAutomation()
        refreshBlockedCalls()
        refreshContactTimeline()
        refreshTypingIndicators()
        applyCurrentFormTemplate()
        // iu8lmc fix (verso 1.0.448): il pannello e' caricato da un Loader asynchronous (Main.qml)
        // -> puo' essere DISTRUTTO durante l'incubazione prima che questa callLater differita scatti
        // ('Object or context destroyed during incubation' + TypeError ... is not a function su
        // loadPresenceEditor/refreshPresence). Guard: esegui solo se il componente e' ancora vivo e
        // il metodo e' callable; altrimenti no-op (il pannello sta morendo, popolarlo e' inutile).
        Qt.callLater(function() {
            if (root && typeof root.loadPresenceEditor === "function")
                root.loadPresenceEditor()
        })
    }

    Timer {
        interval: 1000
        running: root.visible
        repeat: true
        onTriggered: {
            root.uiNowMs = Date.now()
            root.syncLocalStation()
            root.refreshStations()
            root.refreshSessions()
            root.refreshBroadcasts()
            root.refreshAlerts()
            root.refreshAlertTags()
            root.refreshMailbox()
            root.refreshForms()
            root.refreshFileTransfers()
            root.refreshReceivedFiles()
            root.refreshBulletins()
            root.refreshQsoLog()
            root.refreshLogbookOutbox()
            root.refreshContactHistory()
            root.refreshPingLog()
            root.refreshPathReports()
            root.refreshBeaconHistory()
            root.updateClusterFromRig()
            root.refreshPathAnalysis()
            root.refreshStatistics()
            root.refreshStoreAudit()
            root.refreshContactTimeline()
            root.refreshTypingIndicators()
        }
    }

    Timer {
        interval: 30000
        running: root.visible
        repeat: true
        onTriggered: {
            if (!ft2Link)
                return
            var changed = false
            if (typeof ft2Link.evaluateAutoAway === "function") {
                var awayResult = ft2Link.evaluateAutoAway(root.nowMs())
                changed = changed || !!(awayResult && awayResult.changed)
            }
            if (typeof ft2Link.evaluateQsoAutomation === "function") {
                var qsoResult = ft2Link.evaluateQsoAutomation(root.nowMs())
                changed = changed || !!(qsoResult && qsoResult.changed)
            }
            if (changed) {
                root.refreshPresence()
                root.refreshQsoAutomation()
                root.refreshSessions()
                root.refreshMessages()
                root.refreshStatistics()
                root.refreshStoreAudit()
                if (root.toolPageIndex === 6)
                    root.loadPresenceEditor()
            }
        }
    }

    Timer {
        interval: 5000
        running: root.visible && root.clusterAutoSync
        repeat: true
        onTriggered: {
            var now = Date.now()
            if (root.clusterLastAutoSyncMs > 0
                    && now - root.clusterLastAutoSyncMs < root.clusterAutoSyncSeconds * 1000)
                return
            root.clusterLastAutoSyncMs = now
            root.syncClusterShare(false)
        }
    }

    Timer {
        interval: 450
        running: root.chatUnreadBelow
        repeat: true
        onTriggered: root.chatUnreadPulse = !root.chatUnreadPulse
        onRunningChanged: {
            if (!running)
                root.chatUnreadPulse = false
        }
    }

    Connections {
        target: ft2Link
        ignoreUnknownSignals: true
        function onStationCountChanged() { root.refreshStations() }
        function onSessionCountChanged() { root.refreshSessions() }
        function onActiveSessionChanged() {
            root.selectedSessionId = ft2Link.activeSessionId
            root.refreshSessions()
            root.refreshStatistics()
            root.refreshPathAnalysis()
        }
        function onSessionsChanged() { root.refreshSessions(); root.refreshStatistics() }
        function onMessagesChanged(sessionId) {
            if (Number(sessionId) === root.selectedSessionId)
                root.refreshMessages()
            root.refreshContactTimeline()
            root.refreshStatistics()
        }
        function onTransportStateChanged() { root.refreshStatistics(); root.refreshPathAnalysis() }
        function onBroadcastsChanged() { root.refreshBroadcasts(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onAlertsChanged() { root.refreshAlerts(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onAlertTagsChanged() { root.refreshAlertTags(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onMailboxChanged() { root.refreshMailbox(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onFormsChanged() { root.refreshForms(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onFileTransfersChanged() { root.refreshFileTransfers(); root.refreshReceivedFiles(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onBulletinsChanged() { root.refreshBulletins(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onQsoLogChanged() { root.refreshQsoLog(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onLogbookOutboxChanged() { root.refreshLogbookOutbox(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onContactHistoryChanged() { root.refreshContactHistory(); root.refreshContactTimeline(); root.refreshStatistics(); root.refreshPathAnalysis() }
        function onPingLogChanged() { root.refreshPingLog(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onPathReportsChanged() { root.refreshPathReports(); root.refreshPathAnalysis(); root.refreshContactTimeline(); root.refreshStatistics() }
        function onBeaconHistoryChanged() { root.refreshBeaconHistory(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onClusterLastHeardChanged() { root.refreshClusterLastHeard(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onLocalStoreChanged() { root.refreshStoreAudit(); root.refreshPresence(); root.refreshQsoAutomation(); root.refreshBlockedCalls(); root.refreshClusterLastHeard(); root.refreshLogbookOutbox() }
        function onCannedMessagesChanged() { root.refreshCannedMessages(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onFrequencyPlanChanged() { root.refreshFrequencyPlan(); root.refreshStatistics(); root.refreshStoreAudit(); root.refreshQsyPlan() }
        function onBlockListChanged() { root.refreshBlockedCalls(); root.refreshStations(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onQsoAutomationChanged() { root.refreshQsoAutomation(); root.refreshStatistics(); root.refreshStoreAudit() }
        function onTypingIndicatorsChanged() { root.refreshTypingIndicators() }
        function onPresenceChanged() {
            root.refreshPresence()
            root.refreshStatistics()
            root.refreshStoreAudit()
            if (root.toolPageIndex === 6)
                root.loadPresenceEditor()
        }
    }

    Connections {
        target: bridge
        ignoreUnknownSignals: true
        function onExternalAdifUploadStatus(uploadId, state, detail) {
            if (!ft2Link || typeof ft2Link.markLogbookUpload !== "function")
                return
            var cleanState = String(state || "Submitted")
            var cleanDetail = String(detail || "")
            ft2Link.markLogbookUpload(Number(uploadId || 0), cleanState, cleanDetail, root.nowMs())
            root.refreshLogbookOutbox()
            root.refreshStatistics()
            root.refreshStoreAudit()
            root.logExportText = root.prettyJson({
                ok: cleanState === "Sent",
                uploadId: Number(uploadId || 0),
                state: cleanState,
                detail: cleanDetail
            })
        }
        function onFt2LinkEmailGatewayStatus(requestId, mailboxId, state, detail) {
            var cleanState = String(state || "")
            var cleanDetail = String(detail || "")
            if (ft2Link && typeof ft2Link.markMailboxEmailGateway === "function"
                    && Number(mailboxId || 0) > 0) {
                ft2Link.markMailboxEmailGateway(Number(mailboxId || 0),
                                                cleanState,
                                                cleanDetail,
                                                root.nowMs())
                root.refreshMailbox()
                root.refreshStatistics()
                root.refreshStoreAudit()
            }
            root.emailGatewayStatus = "SMTP #" + Number(requestId || 0)
                                      + " " + cleanState
                                      + (cleanDetail.length > 0 ? " - " + cleanDetail : "")
            root.setEmailGatewayItemState(mailboxId, cleanState, cleanDetail)
            root.logExportText = root.prettyJson({
                ok: cleanState === "Sent",
                requestId: Number(requestId || 0),
                mailboxId: Number(mailboxId || 0),
                state: cleanState,
                detail: cleanDetail
            })
        }
    }

    component SmallButton: Rectangle {
        id: smallButton
        signal clicked(var mouse)

        property string text: ""
        property string tip: ""
        property color accent: root.cyan
        property bool danger: false
        property bool hovered: false
        property bool pressed: false
        property bool checked: false
        property bool interactive: true
        property int labelSize: 10
        implicitHeight: 24
        implicitWidth: 64
        radius: 4
        color: !enabled ? Qt.rgba(1, 1, 1, 0.020)
                        : (checked ? Qt.rgba(accent.r, accent.g, accent.b, 0.22)
                        : (pressed ? Qt.rgba(accent.r, accent.g, accent.b, 0.24)
                                   : (hovered ? Qt.rgba(accent.r, accent.g, accent.b, 0.14)
                                              : Qt.rgba(1, 1, 1, 0.035))))
        border.color: checked ? accent
                              : (enabled ? Qt.rgba(accent.r, accent.g, accent.b, 0.58)
                              : Qt.rgba(root.textSecondary.r, root.textSecondary.g, root.textSecondary.b, 0.18)
                                )
        border.width: 1
        opacity: enabled ? 1.0 : 0.72

        Text {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            text: smallButton.text
            color: smallButton.enabled ? smallButton.accent
                                       : Qt.rgba(root.textSecondary.r, root.textSecondary.g, root.textSecondary.b, 0.52)
            font.family: root.mono
            font.pixelSize: smallButton.labelSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        MouseArea {
            anchors.fill: parent
            enabled: smallButton.enabled && smallButton.interactive
            hoverEnabled: true
            cursorShape: smallButton.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
            onEntered: smallButton.hovered = true
            onExited: {
                smallButton.hovered = false
                smallButton.pressed = false
            }
            onPressed: smallButton.pressed = true
            onReleased: smallButton.pressed = false
            onCanceled: smallButton.pressed = false
            onClicked: function(mouse) { smallButton.clicked(mouse) }
        }

        ToolTip.visible: hovered && tip.length > 0
        ToolTip.text: tip
        ToolTip.delay: 450
    }

    component PaneResizeHandle: Rectangle {
        id: resizeHandle

        property string targetPane: "station"
        property bool hovered: false
        property bool pressed: false

        Layout.preferredWidth: 12
        Layout.fillHeight: true
        radius: 4
        color: pressed ? Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.16)
                       : (hovered ? Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.10)
                                  : "transparent")
        border.color: hovered || pressed ? Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.32)
                                          : root.borderSoft
        border.width: hovered || pressed ? 1 : 0

        Column {
            anchors.centerIn: parent
            spacing: 3
            Repeater {
                model: 3
                Rectangle {
                    width: 3
                    height: 3
                    radius: 2
                    color: resizeHandle.hovered || resizeHandle.pressed
                           ? root.cyan : root.textSecondary
                    opacity: resizeHandle.hovered || resizeHandle.pressed ? 0.9 : 0.45
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SplitHCursor
            property real pressX: 0
            property int pressWidth: 0

            onEntered: resizeHandle.hovered = true
            onExited: {
                if (!resizeHandle.pressed)
                    resizeHandle.hovered = false
            }
            onPressed: function(mouse) {
                resizeHandle.pressed = true
                pressX = mouse.x
                pressWidth = resizeHandle.targetPane === "session"
                             ? root.sessionPaneWidth
                             : root.stationPaneWidth
            }
            onPositionChanged: function(mouse) {
                if (!resizeHandle.pressed)
                    return
                var width = pressWidth + (mouse.x - pressX)
                if (resizeHandle.targetPane === "session")
                    root.resizeSessionPane(width)
                else
                    root.resizeStationPane(width)
            }
            onReleased: {
                resizeHandle.pressed = false
                resizeHandle.hovered = containsMouse
            }
            onCanceled: {
                resizeHandle.pressed = false
                resizeHandle.hovered = containsMouse
            }
            onDoubleClicked: {
                if (resizeHandle.targetPane === "session")
                    root.resizeSessionPane(170)
                else
                    root.resizeStationPane(220)
            }
        }

        ToolTip.visible: hovered
        ToolTip.text: targetPane === "session" ? "Resize sessions" : "Resize stations"
        ToolTip.delay: 450
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        RowLayout {
            id: headerRow
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            spacing: 8

            MouseArea {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                cursorShape: root.dragTarget ? Qt.SizeAllCursor : Qt.ArrowCursor
                drag.target: root.dragTarget
                drag.axis: Drag.XAndYAxis
                drag.minimumX: 0
                drag.minimumY: 0
                drag.maximumX: root.dragTarget && root.dragTarget.parent
                               ? Math.max(0, root.dragTarget.parent.width - root.dragTarget.width)
                               : 0
                drag.maximumY: root.dragTarget && root.dragTarget.parent
                               ? Math.max(0, root.dragTarget.parent.height - root.dragTarget.height)
                               : 0
                onReleased: {
                    if (root.dragTarget && root.dragTarget.savePosition)
                        root.dragTarget.savePosition()
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 8

                    Text {
                        text: "FT2-LINK"
                        font.family: root.mono
                        font.pixelSize: 14
                        font.bold: true
                        color: root.cyan
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Rectangle {
                        Layout.preferredWidth: 62
                        Layout.preferredHeight: 20
                        radius: 4
                        color: Qt.rgba(root.green.r, root.green.g, root.green.b, 0.12)
                        border.color: Qt.rgba(root.green.r, root.green.g, root.green.b, 0.46)

                        Text {
                            anchors.centerIn: parent
                            text: root.preferW2300 ? "2300" : "500"
                            font.family: root.mono
                            font.pixelSize: 10
                            font.bold: true
                            color: root.green
                        }
                    }

                    Text {
                        text: ft2Link ? (ft2Link.stationCount + " stn / "
                                         + ft2Link.sessionCount + " sess / "
                                         + ft2Link.broadcastCount + " bc / "
                                         + ft2Link.alertCount + " alert / "
                                         + ft2Link.mailboxCount + " mail"
                                         + (ft2Link.mailboxUnreadCount > 0 ? "+" + ft2Link.mailboxUnreadCount : "")
                                         + " / "
                                         + ft2Link.relayQueueCount + " rly / "
                                         + ft2Link.formCount + " form / "
                                         + ft2Link.fileTransferCount + " file / "
                                         + ft2Link.bulletinCount + " bbs / "
                                         + ft2Link.qsoLogCount + " qso / "
                                         + ft2Link.logbookOutboxCount + " lbq / "
                                         + ft2Link.contactCount + " call / "
                                         + ft2Link.pingCount + " ping / "
                                         + ft2Link.transportState) : "offline"
                        font.family: root.mono
                        font.pixelSize: 10
                        color: ft2Link && ft2Link.alertCount > 0 ? root.red
                              : (ft2Link && ft2Link.transportState === "Failed" ? root.red
                              : (ft2Link && ft2Link.transportBusy ? root.amber : root.textSecondary)
                                )
                        Layout.alignment: Qt.AlignVCenter
                    }
                }
            }

            SmallButton {
                text: root.preferW2300 ? "W2300" : "W500"
                implicitWidth: 76
                accent: root.green
                tip: "Switch 500/2300 Hz profile"
                onClicked: {
                    root.preferW2300 = !root.preferW2300
                    root.applyCapabilities()
                }
            }

            SmallButton {
                text: root.robustMode ? "ROB" : "FAST"
                implicitWidth: 58
                accent: root.amber
                tip: "Switch fast/robust rate"
                onClicked: {
                    root.robustMode = !root.robustMode
                    root.applyCapabilities()
                }
            }

            SmallButton {
                text: "X"
                implicitWidth: 34
                accent: root.red
                tip: "Exit FT2-Link"
                onClicked: root.closeRequested()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.borderSoft
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            radius: 4
            color: Qt.rgba(1, 1, 1, 0.025)
            border.color: Qt.rgba(root.textSecondary.r, root.textSecondary.g, root.textSecondary.b, 0.16)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 12

                Text {
                    Layout.preferredWidth: 190
                    text: root.rfStatusLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    font.bold: ft2Link && ft2Link.transportBusy
                    color: ft2Link && ft2Link.transportBusy ? root.amber : root.textSecondary
                }

                Text {
                    Layout.preferredWidth: 230
                    text: root.queueStatusLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    font.bold: root.queueStatusActive()
                    color: root.queueStatusColor()
                }

                Text {
                    Layout.fillWidth: true
                    text: root.globalErrorLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.red
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            ColumnLayout {
                Layout.preferredWidth: root.stationPaneWidth
                Layout.minimumWidth: 160
                Layout.maximumWidth: 420
                Layout.fillHeight: true
                spacing: 6

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            text: root.stationHistoryMode ? "CQ/BEACON" : "STATIONS"
                            font.family: root.mono
                            font.pixelSize: 11
                            font.bold: true
                            color: root.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        SmallButton {
                            text: "HIST"
                            implicitWidth: 46
                            checked: root.stationHistoryMode
                            accent: root.amber
                            tip: "Toggle CQ/beacon history"
                            onClicked: root.stationHistoryMode = !root.stationHistoryMode
                        }
                        CheckBox {
                            id: cqOnlyCheck
                            visible: !root.stationHistoryMode
                            text: "CQ only"
                            checked: root.cqOnly
                            font.family: root.mono
                            font.pixelSize: 10
                            onToggled: {
                                root.cqOnly = checked
                                root.refreshStations()
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        CheckBox {
                            id: cqArmCheck
                            text: "ARM"
                            checked: ft2Link ? ft2Link.radioTxArmed : false
                            font.family: root.mono
                            font.pixelSize: 10
                            onToggled: {
                                if (ft2Link)
                                    ft2Link.setRadioTxArmed(checked)
                            }
                        }

                        SmallButton {
                            text: root.cqTxButtonText()
                            implicitWidth: 64
                            accent: root.amber
                            enabled: !!ft2Link && ft2Link.radioTxArmed
                            tip: "Transmit one CQ beacon"
                            onClicked: root.transmitBeacon(true)
                        }

                        CheckBox {
                            id: autoBeaconCheck
                            text: "AUTO CQ"
                            checked: ft2Link ? ft2Link.autoBeaconEnabled : false
                            font.family: root.mono
                            font.pixelSize: 10
                            onToggled: root.toggleAutoBeacon(checked)
                        }

                        SmallButton {
                            text: root.beaconIntervalText()
                            implicitWidth: 54
                            accent: root.textSecondary
                            tip: "Auto CQ interval"
                            onClicked: root.cycleBeaconInterval()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 22
                        spacing: 4

                        SmallButton {
                            text: root.currentCqType()
                            implicitWidth: 70
                            accent: root.green
                            tip: "Special CQ type"
                            onClicked: root.cycleCqType()
                        }

                        TextField {
                            id: cqLocatorText
                            Layout.preferredWidth: 64
                            placeholderText: "LOC"
                            text: root.cqLocator
                            font.family: root.mono
                            font.pixelSize: 10
                            maximumLength: 8
                            selectByMouse: true
                            inputMethodHints: Qt.ImhUppercaseOnly
                            onTextEdited: root.cqLocator = text.trim().toUpperCase()
                            onEditingFinished: root.cqLocator = text.trim().toUpperCase()
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.currentCqLocator().length > 0
                                  ? root.currentCqLocator()
                                  : "--"
                            elide: Text.ElideRight
                            font.family: root.mono
                            font.pixelSize: 10
                            color: root.textSecondary
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 22
                        spacing: 4

                        CheckBox {
                            text: "SLOT"
                            checked: !root.skipCqSlot
                            font.family: root.mono
                            font.pixelSize: 10
                            onToggled: root.skipCqSlot = !checked
                        }

                        SmallButton {
                            text: root.currentCqSlotLabel()
                            implicitWidth: 42
                            accent: root.amber
                            enabled: !root.skipCqSlot && root.qsySlots.length > 0
                            tip: root.currentCqSlotTip()
                            onClicked: root.cycleCqSlot()
                        }

                        SmallButton {
                            text: root.cqSlotStatusText()
                            implicitWidth: 62
                            accent: root.cqSlotWaitRemainingSeconds() > 0
                                    ? root.amber
                                    : (root.cqSlotBusy() ? root.red : root.green)
                            enabled: !root.skipCqSlot
                            interactive: false
                            tip: root.currentCqSlotOffsetLabel()
                        }

                        SmallButton {
                            text: root.cqSlotWaitText()
                            implicitWidth: 44
                            accent: root.textSecondary
                            enabled: !root.skipCqSlot
                            tip: "CQ slot wait time"
                            onClicked: root.cycleCqSlotWait()
                        }
                    }
                }

                ListView {
                    id: stationList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.stationHistoryMode ? root.beaconHistory : root.stations
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        width: stationList.width
                        height: 50
                        clip: true
                        color: stationMouse.containsMouse ? root.rowHover : "transparent"
                        border.color: Qt.rgba(root.textSecondary.r, root.textSecondary.g, root.textSecondary.b, 0.10)
                        border.width: 1
                        radius: 3

                        Column {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 2
                            z: 2

	                            Row {
	                                width: parent.width
	                                spacing: 6
		                                Text {
		                                    text: String(modelData.call || "")
		                                    width: 58
		                                    elide: Text.ElideRight
		                                    font.family: root.mono
		                                    font.pixelSize: 12
	                                    font.bold: true
	                                    color: root.cyan
	                                }
		                                Text {
		                                    text: root.stationHistoryMode ? String(modelData.direction || "")
                                                                          : String(modelData.tag || "")
		                                    width: 34
		                                    visible: text.length > 0
		                                    elide: Text.ElideRight
	                                    font.family: root.mono
	                                    font.pixelSize: 10
	                                    font.bold: true
	                                    color: root.stationHistoryMode
                                               ? (String(modelData.direction || "") === "TX" ? root.cyan : root.green)
                                               : root.amber
	                                }
		                                Text {
		                                    text: modelData.cq ? String(modelData.cqType || "CQ") : "BCN"
		                                    width: 48
		                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
		                                    color: modelData.cq ? root.green : root.textSecondary
		                                }
		                                Text {
		                                    text: String(modelData.cqSlotLabel || "")
		                                    width: 34
		                                    visible: text.length > 0
		                                    elide: Text.ElideRight
		                                    font.family: root.mono
		                                    font.pixelSize: 10
		                                    font.bold: true
		                                    color: root.amber
		                                }
		                                Text {
		                                    text: root.stationHistoryMode
                                              ? String(modelData.source || "")
                                              : (modelData.capabilities ? String(modelData.capabilities.preferredProfileName || "") : "")
		                                    width: 48
		                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.amber
                                }
                            }

                            Row {
                                width: parent.width
                                height: 20
                                spacing: 4

                                Text {
                                    text: root.stationSubtitle(modelData)
                                    width: parent.width
                                           - (stationRelayButton.visible ? stationRelayButton.width + parent.spacing : 0)
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.stationSubtitleColor(modelData)
                                }

                                SmallButton {
                                    id: stationRelayButton
                                    readonly property var workflow: root.stationRelayWorkflow(modelData)
                                    text: "FWD"
                                    width: 38
                                    implicitHeight: 18
                                    labelSize: 8
                                    accent: !workflow ? root.amber
                                            : (String(workflow.priority || "") === "EMCOMM" ? root.red : root.amber)
                                    visible: !root.stationHistoryMode
                                             && workflow !== null
                                    enabled: visible
                                    tip: "Call relay and prepare parked mail forwarding"
                                    onClicked: root.useStationRelayWorkflow(modelData)
                                }
                            }
                        }

                        MouseArea {
                            id: stationMouse
                            anchors.fill: parent
                            z: 1
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.startSession(modelData.call)
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: stationList.count === 0
                        text: root.stationHistoryMode ? "No CQ/beacon history"
                                                      : "No FT2-Link stations"
                        font.family: root.mono
                        font.pixelSize: 11
                        color: root.textSecondary
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 4
                    rowSpacing: 4

                    TextField {
                        id: manualCall
                        Layout.fillWidth: true
                        placeholderText: "CALL"
                        font.family: root.mono
                        font.pixelSize: 10
                        selectByMouse: true
                        onAccepted: root.addManualStation()
                    }
                    TextField {
                        id: manualGrid
                        Layout.preferredWidth: 58
                        placeholderText: "GRID"
                        font.family: root.mono
                        font.pixelSize: 10
                        selectByMouse: true
                        onAccepted: root.addManualStation()
                    }
	                    TextField {
	                        id: manualName
	                        Layout.fillWidth: true
	                        placeholderText: "NAME"
                        font.family: root.mono
                        font.pixelSize: 10
	                        selectByMouse: true
	                        onAccepted: root.addManualStation()
	                    }
	                    RowLayout {
	                        spacing: 4
	                        TextField {
	                            id: manualTag
	                            Layout.preferredWidth: 54
	                            placeholderText: "TAG"
	                            font.family: root.mono
	                            font.pixelSize: 10
	                            maximumLength: 16
	                            selectByMouse: true
	                            onAccepted: root.addManualStation()
	                        }
	                        CheckBox {
	                            id: manualCq
                            checked: true
                            text: "CQ"
                            font.family: root.mono
                            font.pixelSize: 10
                        }
                        SmallButton {
                            text: "ADD"
                            implicitWidth: 46
                            accent: root.green
                            tip: "Add manual station"
                            onClicked: root.addManualStation()
                        }
                    }
                }
            }

            PaneResizeHandle {
                targetPane: "station"
            }

            ColumnLayout {
                Layout.preferredWidth: root.sessionPaneWidth
                Layout.minimumWidth: 130
                Layout.maximumWidth: 340
                Layout.fillHeight: true
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "SESSIONS"
                        font.family: root.mono
                        font.pixelSize: 11
                        font.bold: true
                        color: root.textPrimary
                    }
                    Item { Layout.fillWidth: true }
                    SmallButton {
                        text: "LOOP ACK"
                        implicitWidth: 72
                        accent: root.amber
                        visible: root.lastHelloBytes && root.selectedRemoteCall.length > 0
                        enabled: root.lastHelloBytes && root.selectedRemoteCall.length > 0
                        tip: "Complete local loopback handshake"
                        onClicked: root.completeLoopbackAck()
                    }
                }

                ListView {
                    id: sessionList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.sessions
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        width: sessionList.width
                        height: 44
                        radius: 3
                        color: root.selectedSessionId === Number(modelData.sessionId)
                               ? root.rowSelect
                               : (sessionMouse.containsMouse ? root.rowHover : "transparent")

                        Column {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 2
                            Text {
                                text: "#" + Number(modelData.sessionId).toString(16).toUpperCase() + " " + String(modelData.remoteCall || "")
                                width: parent.width
                                elide: Text.ElideRight
                                font.family: root.mono
                                font.pixelSize: 11
                                font.bold: true
                                color: root.textPrimary
                            }
                            Text {
                                text: String(modelData.stateName || "") + "  " + String(modelData.profileName || "")
                                width: parent.width
                                elide: Text.ElideRight
                                font.family: root.mono
                                font.pixelSize: 10
                                color: modelData.stateName === "Closed" ? root.red
                                      : (modelData.accepted ? root.green
                                                            : (modelData.stateName === "Calling" ? root.amber : root.textSecondary))
                            }
                        }

                        MouseArea {
                            id: sessionMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectSession(modelData.sessionId)
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: sessionList.count === 0
                        text: "No sessions"
                        font.family: root.mono
                        font.pixelSize: 11
                        color: root.textSecondary
                    }
                }
            }

            PaneResizeHandle {
                targetPane: "session"
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: root.selectedRemoteCall.length > 0
                              ? root.selectedRemoteCall + (root.selectedSessionStateName.length > 0 ? "  " + root.selectedSessionStateName : "")
                              : "CHAT"
                        font.family: root.mono
                        font.pixelSize: 11
                        font.bold: true
                        color: root.selectedSessionStateName === "Closed" ? root.red : root.textPrimary
                    }
                    Text {
                        visible: root.typingSummaryText.length > 0
                        text: root.typingSummaryText
                        Layout.maximumWidth: 180
                        elide: Text.ElideRight
                        font.family: root.mono
                        font.pixelSize: 10
                        color: root.green
                    }
                    Item { Layout.fillWidth: true }
                    SmallButton {
                        text: ft2Link && ft2Link.radioTxArmed ? "PING TX" : "PING"
                        implicitWidth: 62
                        accent: root.cyan
                        enabled: !!ft2Link && root.selectedRemoteCall.length > 0
                        tip: ft2Link && ft2Link.radioTxArmed ? "Transmit ping" : "Arm ping"
                        onClicked: root.armOrTransmitPing(root.selectedRemoteCall)
                    }
                    SmallButton {
                        text: "DISC"
                        implicitWidth: 46
                        accent: root.amber
                        visible: root.selectedSessionId > 0
                        enabled: root.selectedSessionId > 0 && root.selectedSessionStateName !== "Closed"
                        tip: "Send 73 and disconnect"
                        onClicked: root.disconnectSelectedSession()
                    }
                    SmallButton {
                        text: "ABORT"
                        implicitWidth: 54
                        accent: root.red
                        visible: root.selectedSessionId > 0
                        enabled: root.selectedSessionId > 0 && root.selectedSessionStateName !== "Closed"
                        tip: "Abort selected session locally"
                        onClicked: root.closeSelectedSession()
                    }
                    Text {
                        text: ft2Link && ft2Link.lastError.length > 0 ? ft2Link.lastError : ""
                        Layout.maximumWidth: 220
                        elide: Text.ElideRight
                        font.family: root.mono
                        font.pixelSize: 9
                        color: root.red
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    spacing: 8

                    Text {
                        text: root.transportLine()
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.family: root.mono
                        font.pixelSize: 10
                        color: root.textSecondary
                    }

                    CheckBox {
                        id: ackAudioCheck
                        checked: true
                        text: "ACK"
                        font.family: root.mono
                        font.pixelSize: 9
                    }

                    CheckBox {
                        id: dropDataCheck
                        text: "DROP DATA"
                        font.family: root.mono
                        font.pixelSize: 9
                    }

                    CheckBox {
                        id: dropAckCheck
                        text: "DROP ACK"
                        font.family: root.mono
                        font.pixelSize: 9
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    spacing: 8

                    Text {
                        text: root.radioLine()
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.family: root.mono
                        font.pixelSize: 10
                        color: ft2Link && ft2Link.radioTxArmed ? root.amber : root.textSecondary
                    }

                    CheckBox {
                        id: radioArmCheck
                        checked: ft2Link ? ft2Link.radioTxArmed : false
                        text: "ARM"
                        font.family: root.mono
                        font.pixelSize: 9
                        onToggled: {
                            if (ft2Link)
                                ft2Link.setRadioTxArmed(checked)
                        }
                    }

                    SmallButton {
                        text: "PREP"
                        implicitWidth: 46
                        accent: root.amber
                        enabled: root.selectedSessionConnected && composeText.text.trim().length > 0
                        tip: "Prepare RF audio"
                        onClicked: root.prepareRadioTx()
                    }

                    SmallButton {
                        text: "RF TX"
                        implicitWidth: 52
                        accent: root.red
                        enabled: root.selectedSessionConnected && composeText.text.trim().length > 0
                                 && ft2Link && ft2Link.radioTxArmed
                        tip: "Transmit prepared RF audio"
                        onClicked: root.transmitRadioTx()
                    }

                    SmallButton {
                        text: "QSL"
                        implicitWidth: 42
                        accent: root.green
                        enabled: root.selectedSessionId > 0
                        tip: "Insert QSL card text"
                        onClicked: root.insertQslCard()
                    }

                    SmallButton {
                        text: "ADIF"
                        implicitWidth: 46
                        accent: root.amber
                        enabled: root.selectedSessionId > 0
                        tip: "Copy ADIF record"
                        onClicked: root.copyAdifRecord()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.broadcasts.length > 0
                    text: root.latestBroadcastLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    font.bold: root.alerts.length > 0
                    color: root.alerts.length > 0 ? root.red : root.textSecondary
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.mailbox.length > 0
                    text: root.latestMailboxLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.green
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.forms.length > 0
                    text: root.latestFormLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.amber
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.fileTransfers.length > 0
                    text: root.latestFileLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.cyan
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.bulletins.length > 0
                    text: root.latestBulletinLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.textPrimary
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.qsoLog.length > 0
                    text: root.latestQsoLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.green
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.contactHistory.length > 0
                    text: root.latestContactLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.textSecondary
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.pingLog.length > 0
                    text: root.latestPingLine()
                    elide: Text.ElideRight
                    font.family: root.mono
                    font.pixelSize: 10
                    color: root.cyan
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: messageList
                        anchors.fill: parent
                        clip: true
                        spacing: 4
                        model: root.selectedMessages
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }
                        onContentYChanged: {
                            if (root.messageListAtEnd()) {
                                root.chatScrollPinned = true
                                root.chatUnreadBelow = false
                            } else {
                                root.chatScrollPinned = false
                            }
                        }
                        onMovementEnded: {
                            if (root.messageListAtEnd()) {
                                root.chatScrollPinned = true
                                root.chatUnreadBelow = false
                            }
                        }
                        onCountChanged: {
                            if (root.chatScrollPinned)
                                Qt.callLater(root.scrollChatToEnd)
                        }

                        delegate: Rectangle {
                            width: messageList.width
                            implicitHeight: messageText.implicitHeight + 16
                            radius: 4
                            color: modelData.directionName === "System"
                                   ? Qt.rgba(root.amber.r, root.amber.g, root.amber.b, 0.12)
                                   : (modelData.directionName === "Outgoing"
                                      ? Qt.rgba(root.cyan.r, root.cyan.g, root.cyan.b, 0.10)
                                      : Qt.rgba(root.green.r, root.green.g, root.green.b, 0.10))
                            border.color: Qt.rgba(1, 1, 1, 0.06)

                            Text {
                                id: messageText
                                anchors.fill: parent
                                anchors.margins: 8
                                text: "[" + String(modelData.deliveryName || "") + "] " + String(modelData.text || "")
                                wrapMode: Text.Wrap
                                font.family: root.mono
                                font.pixelSize: 11
                                color: root.textPrimary
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: messageList.count === 0
                            text: root.selectedSessionId > 0 ? "No messages" : "Select a session"
                            font.family: root.mono
                            font.pixelSize: 11
                            color: root.textSecondary
                        }
                    }

                    SmallButton {
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 10
                        text: "DOWN"
                        implicitWidth: 62
                        accent: root.chatUnreadBelow && root.chatUnreadPulse ? root.green : root.cyan
                        visible: root.selectedMessages.length > 0 && !root.messageListAtEnd()
                        tip: root.chatUnreadBelow ? "New messages below" : "Scroll to latest"
                        onClicked: root.scrollChatToEnd()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: root.borderSoft
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    spacing: 6
                    SmallButton {
                        text: "CHAT"
                        implicitWidth: 54
                        checked: root.toolPageIndex === 0
                        accent: root.green
                        onClicked: root.toolPageIndex = 0
                    }
                    SmallButton {
                        text: "FORM"
                        implicitWidth: 54
                        checked: root.toolPageIndex === 1
                        accent: root.amber
                        onClicked: root.toolPageIndex = 1
                    }
                    SmallButton {
                        text: "FILE"
                        implicitWidth: 54
                        checked: root.toolPageIndex === 2
                        accent: root.cyan
                        onClicked: root.toolPageIndex = 2
                    }
                    SmallButton {
                        text: "BBS"
                        implicitWidth: 48
                        checked: root.toolPageIndex === 3
                        accent: root.textPrimary
                        onClicked: root.toolPageIndex = 3
                    }
                    SmallButton {
                        text: "BCAST"
                        implicitWidth: 62
                        checked: root.toolPageIndex === 4
                        accent: root.alerts.length > 0 ? root.red : root.amber
                        onClicked: root.toolPageIndex = 4
                    }
                    SmallButton {
                        text: ft2Link && ft2Link.mailboxUnreadCount > 0 ? "MAIL*" : "MAIL"
                        implicitWidth: 54
                        checked: root.toolPageIndex === 5
                        accent: ft2Link && ft2Link.mailboxUnreadCount > 0 ? root.amber : root.green
                        onClicked: root.toolPageIndex = 5
                    }
                    SmallButton {
                        text: "INFO"
                        implicitWidth: 54
                        checked: root.toolPageIndex === 6
                        accent: root.cyan
                        onClicked: {
                            root.toolPageIndex = 6
                            root.loadPresenceEditor()
                        }
                    }
                    SmallButton {
                        text: "CALL"
                        implicitWidth: 54
                        checked: root.toolPageIndex === 7
                        accent: root.amber
                        onClicked: root.toolPageIndex = 7
                    }
                    SmallButton {
                        text: "STAT"
                        implicitWidth: 54
                        checked: root.toolPageIndex === 10
                        accent: root.green
                        onClicked: root.toolPageIndex = 10
                    }
                    SmallButton {
                        text: "RXF"
                        implicitWidth: 48
                        checked: root.toolPageIndex === 11
                        accent: root.cyan
                        onClicked: root.toolPageIndex = 11
                    }
                    SmallButton {
                        text: "PATH"
                        implicitWidth: 54
                        checked: root.toolPageIndex === 9
                        accent: root.amber
                        onClicked: root.toolPageIndex = 9
                    }
                    SmallButton {
                        text: "LOG"
                        implicitWidth: 48
                        checked: root.toolPageIndex === 12
                        accent: root.textPrimary
                        onClicked: {
                            root.toolPageIndex = 12
                            if (root.logExportText.length === 0)
                                root.exportLog("OPS")
                        }
                    }
                    SmallButton {
                        text: "DB"
                        implicitWidth: 40
                        checked: root.toolPageIndex === 13
                        accent: root.red
                        onClicked: {
                            root.toolPageIndex = 13
                            root.auditStore()
                        }
                    }
                    SmallButton {
                        text: "PRE"
                        implicitWidth: 44
                        checked: root.toolPageIndex === 14
                        accent: root.amber
                        onClicked: root.toolPageIndex = 14
                    }
                    SmallButton {
                        text: "FREQ"
                        implicitWidth: 52
                        checked: root.toolPageIndex === 15
                        accent: root.cyan
                        onClicked: {
                            root.toolPageIndex = 15
                            if (ft2Link) {
                                frequencyPresetText.text = ft2Link.frequencyPresetsText()
                                allowedQsyRangeText.text = ft2Link.allowedQsyRangesText()
                                frequencyScheduleText.text = typeof ft2Link.frequencyScheduleText === "function"
                                                             ? ft2Link.frequencyScheduleText()
                                                             : ""
                            }
                        }
                    }
                    SmallButton {
                        text: "BLK"
                        implicitWidth: 44
                        checked: root.toolPageIndex === 16
                        accent: root.red
                        onClicked: {
                            root.toolPageIndex = 16
                            root.refreshBlockedCalls()
                            if (ft2Link && typeof ft2Link.blockedCallsText === "function")
                                blockedCallsText.text = ft2Link.blockedCallsText()
                        }
                    }
                    SmallButton {
                        text: "CLST"
                        implicitWidth: 50
                        checked: root.toolPageIndex === 8
                        accent: root.green
                        onClicked: {
                            root.toolPageIndex = 8
                            root.updateClusterFromRig()
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                StackLayout {
                    id: toolStack
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(132, Math.min(190, root.height * 0.36))
                    currentIndex: root.toolPageIndex
                    clip: true

                    Item {
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                visible: root.selectedSessionId > 0
                                spacing: 0

                                ListView {
                                    id: cannedList
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    orientation: ListView.Horizontal
                                    spacing: 6
                                    clip: true
                                    boundsBehavior: Flickable.StopAtBounds
                                    model: root.cannedMessages
                                    ScrollBar.horizontal: ScrollBar {
                                        policy: ScrollBar.AsNeeded
                                    }

                                    delegate: SmallButton {
                                        required property var modelData
                                        text: String(modelData.label || "")
                                        implicitWidth: Math.max(44, Math.min(82, text.length * 9 + 18))
                                        accent: root.cyan
                                        tip: String(modelData.tip || "")
                                        onClicked: root.insertCannedMessage(String(modelData.templateText || ""))
                                    }
                                }

                                Text {
                                    visible: cannedList.count === 0
                                    text: "No tags"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                visible: root.selectedSessionId > 0
                                spacing: 6

                                Text {
                                    text: root.qsyPlanText()
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.qsyPlanValid() ? root.amber : root.textSecondary
                                }

                                SmallButton {
                                    text: "OK"
                                    implicitWidth: 34
                                    accent: root.green
                                    enabled: root.selectedSessionConnected && root.qsyPlanValid()
                                    tip: "Send QSY accepted"
                                    onClicked: root.sendQsyControlTag(String(root.qsyPlan.acceptTag || "<QSYR>"))
                                }

                                SmallButton {
                                    text: "NO"
                                    implicitWidth: 34
                                    accent: root.amber
                                    enabled: root.selectedSessionConnected && root.qsyPlanValid()
                                    tip: "Send QSY rejected"
                                    onClicked: root.sendQsyControlTag(String(root.qsyPlan.rejectTag || "<QSYJ>"))
                                }

                                SmallButton {
                                    text: "QJO"
                                    implicitWidth: 40
                                    accent: root.red
                                    enabled: root.selectedSessionConnected && root.qsyPlanValid()
                                    tip: "Reject out-of-range QSY"
                                    onClicked: root.sendQsyControlTag(String(root.qsyPlan.outOfRangeTag || "<QJO>"))
                                }

                                SmallButton {
                                    text: "SET CF"
                                    implicitWidth: 58
                                    accent: root.cyan
                                    enabled: root.currentDialFrequencyHz() > 0
                                    tip: "Store current dial as calling frequency"
                                    onClicked: root.setCallingFrequencyFromRig()
                                }

                                SmallButton {
                                    text: "CF TAG"
                                    implicitWidth: 58
                                    accent: root.cyan
                                    enabled: root.selectedSessionId > 0 && root.qsyCallingFrequencyHz > 0
                                    tip: root.qsyCallingFrequencyText()
                                    onClicked: root.insertCallingFrequencyQsyTag()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                spacing: 6

                                SmallButton {
                                    text: root.currentQsySlotLabel()
                                    implicitWidth: 62
                                    accent: root.amber
                                    enabled: root.selectedSessionId > 0 && root.qsySlots.length > 0
                                    tip: "Cycle QSY slot offset"
                                    onClicked: root.cycleQsySlot()
                                }

                                SmallButton {
                                    text: "QSY"
                                    implicitWidth: 44
                                    accent: root.amber
                                    enabled: root.selectedSessionId > 0 && root.qsySlots.length > 0
                                    tip: root.currentQsySlotTip()
                                    onClicked: root.insertQsySlotTag()
                                }

                                TextField {
                                    id: composeText
                                    Layout.fillWidth: true
                                    placeholderText: "Message"
                                    enabled: true
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    selectByMouse: true
                                    onAccepted: root.sendChatText()
                                }

                                SmallButton {
                                    text: "TX"
                                    implicitWidth: 46
                                    accent: root.green
                                    enabled: root.selectedSessionConnected && composeText.text.trim().length > 0
                                             && (!ft2Link || !ft2Link.transportBusy)
                                    tip: "Send local audio test"
                                    onClicked: root.sendChatText()
                                }
		                            }
		                        }
			                    }

			                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            SmallButton {
                                text: root.currentFormLabel()
                                implicitWidth: 72
                                accent: root.amber
                                enabled: root.formTemplates.length > 0
                                tip: "Cycle form template"
                                onClicked: root.cycleFormTemplate()
                            }

                            TextField {
                                id: formToText
                                Layout.preferredWidth: 86
                                placeholderText: root.selectedRemoteCall.length > 0 ? root.selectedRemoteCall : "TO"
                                enabled: root.selectedSessionConnected
                                font.family: root.mono
                                font.pixelSize: 11
                                maximumLength: 16
                                selectByMouse: true
                            }

                            TextField {
                                id: formFieldsText
                                Layout.fillWidth: true
                                placeholderText: "key=value; key=value"
                                enabled: root.selectedSessionConnected
                                font.family: root.mono
                                font.pixelSize: 11
                                maximumLength: 512
                                selectByMouse: true
                                onAccepted: root.armOrTransmitForm()
                            }

                            SmallButton {
                                text: ft2Link && ft2Link.radioTxArmed ? "FORM TX" : "ARM F"
                                implicitWidth: 74
                                accent: root.amber
                                enabled: !!ft2Link && root.selectedSessionConnected
                                         && formFieldsText.text.trim().length > 0
                                tip: ft2Link && ft2Link.radioTxArmed ? "Transmit form" : "Arm form transmit"
                                onClicked: root.armOrTransmitForm()
                            }
                        }
                    }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            TextField {
                                id: fileToText
                                Layout.preferredWidth: 86
                                placeholderText: root.selectedRemoteCall.length > 0 ? root.selectedRemoteCall : "TO"
                                enabled: root.selectedSessionConnected
                                font.family: root.mono
                                font.pixelSize: 11
                                maximumLength: 16
                                selectByMouse: true
                            }

                            TextField {
                                id: fileNameText
                                Layout.preferredWidth: 132
                                placeholderText: "file.txt"
                                enabled: root.selectedSessionConnected
                                font.family: root.mono
                                font.pixelSize: 11
                                maximumLength: 64
                                selectByMouse: true
                            }

                            TextField {
                                id: fileContentText
                                Layout.fillWidth: true
                                placeholderText: "Small file text"
                                enabled: root.selectedSessionConnected
                                font.family: root.mono
                                font.pixelSize: 11
                                maximumLength: 4096
                                selectByMouse: true
                                onAccepted: root.armOrTransmitFile()
                            }

                            SmallButton {
                                text: ft2Link && ft2Link.radioTxArmed ? "FILE TX" : "ARM X"
                                implicitWidth: 72
                                accent: root.cyan
                                enabled: !!ft2Link && root.selectedSessionConnected
                                         && fileContentText.text.trim().length > 0
                                tip: ft2Link && ft2Link.radioTxArmed ? "Transmit small file" : "Arm file transmit"
                                onClicked: root.armOrTransmitFile()
                            }
                        }
                    }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            TextField {
                                id: bulletinGroupText
                                Layout.preferredWidth: 72
                                placeholderText: "ALL"
                                enabled: root.selectedSessionConnected
                                font.family: root.mono
                                font.pixelSize: 11
                                maximumLength: 16
                                selectByMouse: true
                            }

                            TextField {
                                id: bulletinTitleText
                                Layout.preferredWidth: 140
                                placeholderText: "Bulletin"
                                enabled: root.selectedSessionConnected
                                font.family: root.mono
                                font.pixelSize: 11
                                maximumLength: 64
                                selectByMouse: true
                            }

                            TextField {
                                id: bulletinBodyText
                                Layout.fillWidth: true
                                placeholderText: "BBS message"
                                enabled: root.selectedSessionConnected
                                font.family: root.mono
                                font.pixelSize: 11
                                maximumLength: 1024
                                selectByMouse: true
                                onAccepted: root.armOrTransmitBulletin()
                            }

                            SmallButton {
                                text: ft2Link && ft2Link.radioTxArmed ? "BBS TX" : "ARM BBS"
                                implicitWidth: 78
                                accent: root.textPrimary
                                enabled: !!ft2Link && root.selectedSessionConnected
                                         && bulletinBodyText.text.trim().length > 0
                                tip: ft2Link && ft2Link.radioTxArmed ? "Transmit bulletin" : "Arm bulletin transmit"
                                onClicked: root.armOrTransmitBulletin()
                            }
                        }
                    }

                    Item {
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 5

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 6

                                TextField {
                                    id: broadcastText
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    placeholderText: "Broadcast"
                                    enabled: true
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 32
                                    selectByMouse: true
                                    onAccepted: root.armOrTransmitBroadcast()
                                }

                                SmallButton {
                                    text: ft2Link && ft2Link.radioTxArmed ? "BCAST TX" : "ARM B"
                                    implicitWidth: 68
                                    accent: root.alerts.length > 0 ? root.red : root.amber
                                    enabled: !!ft2Link && broadcastText.text.trim().length > 0
                                    tip: ft2Link && ft2Link.radioTxArmed ? "Transmit broadcast" : "Arm broadcast transmit"
                                    onClicked: root.armOrTransmitBroadcast()
                                }

                                TextField {
                                    id: pathTargetText
                                    Layout.preferredWidth: 88
                                    Layout.preferredHeight: 28
                                    placeholderText: "PATH CALL"
                                    enabled: true
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 16
                                    selectByMouse: true
                                    onAccepted: root.armOrTransmitPathFinderRequest()
                                }

                                SmallButton {
                                    text: ft2Link && ft2Link.radioTxArmed ? "PATH TX" : "PATH?"
                                    implicitWidth: 68
                                    accent: root.cyan
                                    enabled: !!ft2Link && pathTargetText.text.trim().length > 0
                                    tip: "Send path finder request"
                                    onClicked: root.armOrTransmitPathFinderRequest()
                                }

                                SmallButton {
                                    text: ft2Link && ft2Link.radioTxArmed ? "PATH TX" : "PATH!"
                                    implicitWidth: 68
                                    accent: root.amber
                                    enabled: !!ft2Link && root.pathFinderCandidate() !== null
                                    tip: "Reply that target was heard recently"
                                    onClicked: root.armOrTransmitPathFinderResponse()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 6

                                TextField {
                                    id: alertTagsText
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    text: root.alertTagList.join(", ")
                                    placeholderText: "Alert tags: WX, POTA, NET"
                                    enabled: true
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 256
                                    selectByMouse: true
                                    onAccepted: root.saveAlertTags()
                                }

                                SmallButton {
                                    text: "SAVE"
                                    implicitWidth: 52
                                    accent: root.green
                                    enabled: !!ft2Link
                                    tip: "Save custom alert tags"
                                    onClicked: root.saveAlertTags()
                                }

                                SmallButton {
                                    text: "CLR"
                                    implicitWidth: 44
                                    accent: root.red
                                    enabled: !!ft2Link && root.alertTagList.length > 0
                                    tip: "Clear custom alert tags"
                                    onClicked: root.clearAlertTags()
                                }

                                Text {
                                    Layout.preferredWidth: 90
                                    text: String(root.alertTagList.length) + " custom"
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                spacing: 6

                                Text {
                                    Layout.fillWidth: true
                                    text: root.pathRelayLine()
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.pathRelayHint() ? root.amber : root.textSecondary
                                }

                                SmallButton {
                                    text: "MAIL"
                                    implicitWidth: 48
                                    accent: root.green
                                    enabled: root.pathRelayHint() !== null
                                    tip: "Use relay hint as mailbox target"
                                    onClicked: root.usePathRelayForMail()
                                }

                                SmallButton {
                                    text: "CALL"
                                    implicitWidth: 48
                                    accent: root.cyan
                                    enabled: root.pathRelayHint() !== null
                                    tip: "Start session with suggested relay"
                                    onClicked: root.callPathRelay()
                                }

                                SmallButton {
                                    readonly property var relayHint: root.pathRelayHint()
                                    text: "FWD"
                                    implicitWidth: 46
                                    accent: root.amber
                                    enabled: relayHint !== null && !!relayHint.readyToForward
                                    tip: "Prepare parked mail and call relay"
                                    onClicked: root.forwardPathRelay()
                                }
                            }
		                    }
	                    }

		                    Item {
		                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                spacing: 6

                                TextField {
                                    id: mailToText
                                    Layout.preferredWidth: 86
                                    placeholderText: root.selectedRemoteCall.length > 0 ? root.selectedRemoteCall : "TO"
                                    enabled: true
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 16
                                    selectByMouse: true
                                }

                                TextField {
                                    id: mailSubjectText
                                    Layout.preferredWidth: 150
                                    placeholderText: "Subject"
                                    enabled: true
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 48
                                    selectByMouse: true
                                }

                                TextField {
                                    id: mailBodyText
                                    Layout.fillWidth: true
                                    placeholderText: "Mail body"
                                    enabled: true
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 512
                                    selectByMouse: true
                                    onAccepted: root.armOrTransmitMailbox()
                                }

                                SmallButton {
                                    text: ft2Link && ft2Link.radioTxArmed ? "MAIL TX" : "ARM M"
                                    implicitWidth: 72
                                    accent: root.green
                                    enabled: !!ft2Link && root.selectedSessionConnected
                                             && mailBodyText.text.trim().length > 0
                                    tip: ft2Link && ft2Link.radioTxArmed ? "Transmit mailbox item" : "Arm mailbox transmit"
                                    onClicked: root.armOrTransmitMailbox()
                                }

                                SmallButton {
                                    text: "PARK"
                                    implicitWidth: 62
                                    accent: root.cyan
                                    enabled: !!ft2Link
                                             && mailToText.text.trim().length > 0
                                             && mailBodyText.text.trim().length > 0
                                    tip: "Park mail for relay notification"
                                    onClicked: root.parkMailboxText()
                                }

                                SmallButton {
                                    text: root.relayMailboxButtonText()
                                    implicitWidth: 78
                                    accent: root.amber
                                    enabled: !!ft2Link && root.selectedSessionConnected
                                             && root.relayMailboxCandidate() !== null
                                    tip: "Relay parked mail for this session"
                                    onClicked: root.armOrTransmitRelayMailbox()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                spacing: 6

                                CheckBox {
                                    id: mailUrgentCheck
                                    text: "URG"
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                CheckBox {
                                    id: mailEmcommCheck
                                    text: "EMC"
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: ft2Link ? ("UNREAD " + ft2Link.mailboxUnreadCount
                                                     + " / " + ft2Link.mailboxCount
                                                     + "   RLY " + ft2Link.relayQueueCount) : "UNREAD --"
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    font.bold: ft2Link && (ft2Link.mailboxUnreadCount > 0 || ft2Link.relayQueueCount > 0)
                                    color: ft2Link && ft2Link.mailboxUnreadCount > 0 ? root.green
                                           : (ft2Link && ft2Link.relayQueueCount > 0 ? root.amber : root.textSecondary)
                                }

                                SmallButton {
                                    text: "COPY"
                                    implicitWidth: 52
                                    accent: root.cyan
                                    enabled: !!ft2Link && root.mailbox.length > 0
                                    tip: "Copy printable mailbox export"
                                    onClicked: root.copyMailboxText()
                                }

                                SmallButton {
                                    text: "RLYQ"
                                    implicitWidth: 48
                                    accent: root.amber
                                    enabled: !!ft2Link && root.relayQueue.length > 0
                                    tip: "Copy relay queue export"
                                    onClicked: root.copyRelayQueueText()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                spacing: 5

                                CheckBox {
                                    text: "GW"
                                    checked: root.emailGatewayEnabled
                                    font.family: root.mono
                                    font.pixelSize: 9
                                    onToggled: root.emailGatewayEnabled = checked
                                }

                                TextField {
                                    Layout.preferredWidth: 150
                                    Layout.preferredHeight: 22
                                    text: root.emailGatewayHost
                                    placeholderText: "smtp.host"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    selectByMouse: true
                                    onEditingFinished: root.emailGatewayHost = text.trim()
                                    onAccepted: root.emailGatewayHost = text.trim()
                                }

                                TextField {
                                    Layout.preferredWidth: 48
                                    Layout.preferredHeight: 22
                                    text: String(root.emailGatewayPort)
                                    placeholderText: "587"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    horizontalAlignment: TextInput.AlignHCenter
                                    validator: IntValidator { bottom: 1; top: 65535 }
                                    onEditingFinished: {
                                        var value = Number(text)
                                        if (isFinite(value))
                                            root.emailGatewayPort = Math.max(1, Math.min(65535, Math.round(value)))
                                        text = String(root.emailGatewayPort)
                                    }
                                    onAccepted: {
                                        var value = Number(text)
                                        if (isFinite(value))
                                            root.emailGatewayPort = Math.max(1, Math.min(65535, Math.round(value)))
                                        text = String(root.emailGatewayPort)
                                    }
                                }

                                SmallButton {
                                    text: root.emailGatewaySecurity()
                                    implicitWidth: 70
                                    accent: root.textSecondary
                                    tip: "Cycle SMTP security"
                                    onClicked: root.cycleEmailGatewaySecurity()
                                }

                                TextField {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 22
                                    text: root.emailGatewayUsername
                                    placeholderText: "SMTP user"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    selectByMouse: true
                                    onEditingFinished: root.emailGatewayUsername = text.trim()
                                    onAccepted: root.emailGatewayUsername = text.trim()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                spacing: 5

                                TextField {
                                    Layout.preferredWidth: 185
                                    Layout.preferredHeight: 22
                                    text: root.emailGatewayFrom
                                    placeholderText: "From email"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    selectByMouse: true
                                    onEditingFinished: root.emailGatewayFrom = text.trim()
                                    onAccepted: root.emailGatewayFrom = text.trim()
                                }

                                TextField {
                                    id: emailGatewayPasswordText
                                    Layout.preferredWidth: 150
                                    Layout.preferredHeight: 22
                                    placeholderText: "SMTP password"
                                    echoMode: TextInput.Password
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    selectByMouse: true
                                    onAccepted: root.saveEmailGatewayPassword()
                                }

                                SmallButton {
                                    text: "SAVE"
                                    implicitWidth: 46
                                    accent: root.green
                                    enabled: !!bridge && emailGatewayPasswordText.text.length > 0
                                    tip: "Save SMTP password in secure storage"
                                    onClicked: root.saveEmailGatewayPassword()
                                }

                                SmallButton {
                                    text: "CLR"
                                    implicitWidth: 38
                                    accent: root.red
                                    enabled: !!bridge
                                    tip: "Clear saved SMTP password"
                                    onClicked: root.clearEmailGatewayPassword()
                                }

                                SmallButton {
                                    text: "TEST"
                                    implicitWidth: 46
                                    accent: root.amber
                                    enabled: !!bridge && root.emailGatewayEnabled
                                             && root.emailGatewayHost.trim().length > 0
                                    tip: "Test SMTP connect, TLS and authentication without sending mail"
                                    onClicked: root.testEmailGateway()
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.emailGatewayStatus.length > 0
                                          ? root.emailGatewayStatus
                                          : "SMTP gateway idle"
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.emailGatewayStatus.indexOf("error") >= 0
                                           || root.emailGatewayStatus.indexOf("Failed") >= 0
                                           ? root.red
                                           : root.textSecondary
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            ListView {
                                id: mailboxList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 2
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.mailbox
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Item {
                                    id: mailDelegate
                                    required property var modelData
                                    readonly property string mailDirection: String(modelData.direction || "")
                                    readonly property string mailState: String(modelData.state || "--")
                                    readonly property string peer: mailDirection === "Incoming"
                                                                   ? String(modelData.fromCall || "--")
                                                                   : String(modelData.toCall || "--")
                                    readonly property string priority: String(modelData.priority || "NORMAL")
                                    width: mailboxList.width
                                    height: 28

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 5

                                        Text {
                                            Layout.preferredWidth: 62
                                            text: mailDelegate.mailState
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: mailDelegate.modelData.unread ? root.green
                                                                                 : (mailDelegate.mailState === "Read" ? root.textSecondary : root.amber)
                                        }

                                        Text {
                                            Layout.preferredWidth: 58
                                            text: mailDelegate.priority === "NORMAL" ? "--" : mailDelegate.priority
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: mailDelegate.priority !== "NORMAL"
                                            color: mailDelegate.modelData.urgent ? root.red
                                                                                 : (mailDelegate.modelData.emcomm ? root.amber : root.textSecondary)
                                        }

                                        Text {
                                            Layout.preferredWidth: 54
                                            text: mailDelegate.modelData.relayEnvelope
                                                  ? "RLY" + String(mailDelegate.modelData.relayHopCount || 0)
                                                  : (String(mailDelegate.modelData.direction || "") === "Relay" ? "RLY" : "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: mailDelegate.modelData.relayEnvelope ? root.amber : root.textSecondary
                                        }

                                        Text {
                                            Layout.preferredWidth: 76
                                            text: mailDelegate.modelData.relayEnvelope
                                                  && String(mailDelegate.modelData.relayViaCall || "").length > 0
                                                  ? String(mailDelegate.modelData.relayViaCall || "--")
                                                  : mailDelegate.peer
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(mailDelegate.modelData.subject || "") + "  " + String(mailDelegate.modelData.body || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        SmallButton {
                                            text: mailDelegate.mailState === "Read" ? "NEW" : "READ"
                                            implicitWidth: 48
                                            accent: root.green
                                            enabled: !!ft2Link && mailDelegate.mailDirection === "Incoming"
                                            tip: mailDelegate.mailState === "Read" ? "Mark mail as new" : "Mark mail as read"
                                            onClicked: root.markMailboxItemRead(mailDelegate.modelData, mailDelegate.mailState !== "Read")
                                        }

                                        SmallButton {
                                            text: "EMAIL"
                                            implicitWidth: 54
                                            accent: root.cyan
                                            enabled: !!ft2Link
                                            tip: "Open as email draft or copy EML"
                                            onClicked: root.openMailboxEmail(mailDelegate.modelData)
                                        }

                                        SmallButton {
                                            text: root.emailGatewayItemState(mailDelegate.modelData) === "Sent"
                                                  ? "SENT" : "SMTP"
                                            implicitWidth: 48
                                            accent: root.emailGatewayItemState(mailDelegate.modelData) === "Failed"
                                                    ? root.red
                                                    : (root.emailGatewayItemState(mailDelegate.modelData) === "Sent"
                                                       ? root.green
                                                       : root.cyan)
                                            enabled: !!bridge && root.emailGatewayEnabled
                                            tip: "Send this VMail through configured SMTP gateway"
                                            onClicked: root.sendMailboxGatewayEmail(mailDelegate.modelData)
                                        }

                                        SmallButton {
                                            text: "EML"
                                            implicitWidth: 42
                                            accent: root.amber
                                            enabled: !!ft2Link
                                            tip: "Save mailbox item as .eml"
                                            onClicked: root.saveMailboxEml(mailDelegate.modelData)
                                        }

                                        SmallButton {
                                            text: "DEL"
                                            implicitWidth: 42
                                            accent: root.red
                                            enabled: !!ft2Link
                                            tip: "Delete mailbox item"
                                            onClicked: root.deleteMailboxItem(mailDelegate.modelData)
                                        }
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: mailboxList.count === 0
                                    text: "No mail"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }
                        }
                    }

                    Item {
                        Flickable {
                            anchors.fill: parent
                            clip: true
                            contentWidth: width
                            contentHeight: infoColumn.implicitHeight
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            ColumnLayout {
                                id: infoColumn
                                width: parent.width
                                spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                spacing: 6

                                TextField {
                                    Layout.preferredWidth: 92
                                    text: root.profileName
                                    placeholderText: "NAME"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 32
                                    selectByMouse: true
                                    onEditingFinished: root.profileName = text.trim()
                                }

                                TextField {
                                    Layout.preferredWidth: 116
                                    text: root.profileQth
                                    placeholderText: "QTH"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 64
                                    selectByMouse: true
                                    onEditingFinished: root.profileQth = text.trim()
                                }

                                TextField {
                                    Layout.fillWidth: true
                                    text: root.profileEmail
                                    placeholderText: "EMAIL"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 96
                                    selectByMouse: true
                                    onEditingFinished: root.profileEmail = text.trim()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                spacing: 6

                                TextField {
                                    Layout.preferredWidth: 132
                                    text: root.profileRig
                                    placeholderText: "RIG"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 96
                                    selectByMouse: true
                                    onEditingFinished: root.profileRig = text.trim()
                                }

                                TextField {
                                    Layout.preferredWidth: 132
                                    text: root.profileAntenna
                                    placeholderText: "ANT"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 96
                                    selectByMouse: true
                                    onEditingFinished: root.profileAntenna = text.trim()
                                }

                                TextField {
                                    Layout.preferredWidth: 70
                                    text: root.profilePower
                                    placeholderText: "PWR"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 32
                                    selectByMouse: true
                                    onEditingFinished: root.profilePower = text.trim()
                                }

                                TextField {
                                    Layout.fillWidth: true
                                    text: root.profileIce
                                    placeholderText: "ICE"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 128
                                    selectByMouse: true
                                    onEditingFinished: root.profileIce = text.trim()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                spacing: 6

                                TextField {
                                    Layout.fillWidth: true
                                    text: root.profileGps
                                    placeholderText: "GPS"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 96
                                    selectByMouse: true
	                                    onEditingFinished: root.profileGps = text.trim()
	                            }
	                        }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                spacing: 6

                                CheckBox {
                                    id: awayCheck
                                    text: "AWAY"
                                    enabled: !!ft2Link
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                CheckBox {
                                    id: awayQsyCheck
                                    text: "QSY"
                                    enabled: !!ft2Link && awayCheck.checked
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                TextField {
                                    id: awayMessageText
                                    Layout.preferredWidth: 176
                                    placeholderText: "Away message"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 240
                                    selectByMouse: true
                                    enabled: !!ft2Link
                                    onAccepted: root.savePresence()
                                }

                                CheckBox {
                                    id: welcomeCheck
                                    text: "WELCOME"
                                    enabled: !!ft2Link
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                TextField {
                                    id: welcomeMessageText
                                    Layout.fillWidth: true
                                    placeholderText: "Welcome message"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 240
                                    selectByMouse: true
                                    enabled: !!ft2Link
                                    onAccepted: root.savePresence()
                                }

                                CheckBox {
                                    id: autoReplyCheck
                                    text: "AUTO REPLY"
                                    enabled: !!ft2Link
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                SmallButton {
                                    text: "SAVE"
                                    implicitWidth: 52
                                    accent: root.green
                                    enabled: !!ft2Link
                                    tip: "Save away and welcome settings"
                                    onClicked: root.savePresence()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                spacing: 6

                                CheckBox {
                                    id: autoAwayCheck
                                    text: "AUTO AWAY"
                                    enabled: !!ft2Link
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                TextField {
                                    id: autoAwayMinutesText
                                    Layout.preferredWidth: 58
                                    text: "10"
                                    placeholderText: "MIN"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 3
                                    selectByMouse: true
                                    enabled: !!ft2Link && autoAwayCheck.checked
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    validator: IntValidator { bottom: 1; top: 240 }
                                    horizontalAlignment: TextInput.AlignHCenter
                                    onAccepted: root.savePresence()
                                }

                                Text {
                                    Layout.preferredWidth: 96
                                    text: root.presenceState.autoAwayActive ? "AUTO AWAY ACTIVE" : "AUTO AWAY IDLE"
                                    color: root.presenceState.autoAwayActive ? root.amber : root.textSecondary
                                    font.family: root.mono
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }

                                Item {
                                    Layout.fillWidth: true
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                spacing: 6

                                Text {
                                    text: "CALL ID"
                                    color: root.textSecondary
                                    font.family: root.mono
                                    font.pixelSize: 9
                                    Layout.preferredWidth: 54
                                }

                                TextField {
                                    id: callIdIntervalText
                                    Layout.preferredWidth: 58
                                    text: "0"
                                    placeholderText: "MIN"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 3
                                    selectByMouse: true
                                    enabled: !!ft2Link
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    validator: IntValidator { bottom: 0; top: 240 }
                                    horizontalAlignment: TextInput.AlignHCenter
                                    onAccepted: root.saveQsoAutomation()
                                }

                                Text {
                                    text: "AUTO DISC"
                                    color: root.textSecondary
                                    font.family: root.mono
                                    font.pixelSize: 9
                                    Layout.preferredWidth: 66
                                }

                                TextField {
                                    id: autoDisconnectText
                                    Layout.preferredWidth: 58
                                    text: "0"
                                    placeholderText: "MIN"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    maximumLength: 3
                                    selectByMouse: true
                                    enabled: !!ft2Link
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    validator: IntValidator { bottom: 0; top: 240 }
                                    horizontalAlignment: TextInput.AlignHCenter
                                    onAccepted: root.saveQsoAutomation()
                                }

                                CheckBox {
                                    id: incomingPingCheck
                                    text: "PING RX"
                                    checked: true
                                    enabled: !!ft2Link
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: String(root.qsoAutomationState.callIdText || "")
                                    color: root.textSecondary
                                    font.family: root.mono
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                spacing: 6

                                Text {
                                    text: "PRIV"
                                    color: root.textSecondary
                                    font.family: root.mono
                                    font.pixelSize: 9
                                    Layout.preferredWidth: 36
                                }

                                SmallButton {
                                    text: "OPEN"
                                    implicitWidth: 54
                                    checked: root.privacyPresetName() === "OPEN"
                                    accent: root.green
                                    enabled: !!ft2Link
                                    tip: "Publish last-heard, connections, VMail hints, SNR and info replies"
                                    onClicked: root.applyPrivacyPreset("OPEN")
                                }

                                SmallButton {
                                    text: "CONTROL"
                                    implicitWidth: 76
                                    checked: root.privacyPresetName() === "CONTROL"
                                    accent: root.amber
                                    enabled: !!ft2Link
                                    tip: "Keep operations active but hide last-connections and parked VMail peek"
                                    onClicked: root.applyPrivacyPreset("CONTROL")
                                }

                                SmallButton {
                                    text: "QUIET"
                                    implicitWidth: 58
                                    checked: root.privacyPresetName() === "QUIET"
                                    accent: root.red
                                    enabled: !!ft2Link
                                    tip: "Disable automatic disclosure, inbound pings and relay parking"
                                    onClicked: root.applyPrivacyPreset("QUIET")
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.privacySummaryText()
                                    color: root.privacyPresetName() === "QUIET" ? root.red
                                           : (root.privacyPresetName() === "CONTROL" ? root.amber : root.textSecondary)
                                    font.family: root.mono
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                spacing: 6

                                CheckBox {
                                    id: lastHeardPeekingCheck
                                    text: "LH PEEK"
                                    checked: true
                                    enabled: !!ft2Link
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                CheckBox {
                                    id: snrReportCheck
                                    text: "SNR TX"
                                    checked: true
                                    enabled: !!ft2Link
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                CheckBox {
                                    id: verboseSnrAcceptCheck
                                    text: "VSNR OK"
                                    checked: false
                                    enabled: !!ft2Link
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                CheckBox {
                                    id: lastConnectionsPeekingCheck
                                    text: "LC PEEK"
                                    checked: true
                                    enabled: !!ft2Link
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                CheckBox {
                                    id: parkedVmailPeekingCheck
                                    text: "VM PEEK"
                                    checked: true
                                    enabled: !!ft2Link
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                CheckBox {
                                    id: vmailParkingCheck
                                    text: "VM PARK"
                                    checked: true
                                    enabled: !!ft2Link
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                CheckBox {
                                    id: infoInquireCheck
                                    text: "INFO REQ"
                                    checked: true
                                    enabled: !!ft2Link
                                    font.family: root.mono
                                    font.pixelSize: 9
                                }

                                SmallButton {
                                    text: "SAVE"
                                    implicitWidth: 52
                                    accent: root.green
                                    enabled: !!ft2Link
                                    tip: "Save QSO automation toggles"
                                    onClicked: root.saveQsoAutomation()
                                }

	                                Item {
	                                    Layout.fillWidth: true
	                                }
	                            }
	                        }
		                    }

		                    Item {
	                        ColumnLayout {
	                            anchors.fill: parent
	                            spacing: 4

	                            RowLayout {
	                                Layout.fillWidth: true
	                                Layout.preferredHeight: 28
	                                spacing: 5

	                                TextField {
	                                    id: contactCallText
	                                    Layout.preferredWidth: 82
	                                    placeholderText: "CALL"
	                                    font.family: root.mono
	                                    font.pixelSize: 10
	                                    maximumLength: 16
	                                    selectByMouse: true
	                                    onAccepted: root.saveContactDetails()
	                                }

	                                TextField {
	                                    id: contactGridText
	                                    Layout.preferredWidth: 66
	                                    placeholderText: "GRID"
	                                    font.family: root.mono
	                                    font.pixelSize: 10
	                                    maximumLength: 12
	                                    selectByMouse: true
	                                    onAccepted: root.saveContactDetails()
	                                }

	                                TextField {
	                                    id: contactNameText
	                                    Layout.preferredWidth: 116
	                                    placeholderText: "NAME"
	                                    font.family: root.mono
	                                    font.pixelSize: 10
	                                    maximumLength: 48
	                                    selectByMouse: true
	                                    onAccepted: root.saveContactDetails()
	                                }

	                                TextField {
	                                    id: contactTagText
	                                    Layout.preferredWidth: 58
	                                    placeholderText: "TAG"
	                                    font.family: root.mono
	                                    font.pixelSize: 10
	                                    maximumLength: 16
	                                    selectByMouse: true
	                                    onAccepted: root.saveContactDetails()
	                                }

	                                TextField {
	                                    id: contactCommentText
	                                    Layout.fillWidth: true
	                                    placeholderText: "Comment"
	                                    font.family: root.mono
	                                    font.pixelSize: 10
	                                    maximumLength: 240
	                                    selectByMouse: true
	                                    onAccepted: root.saveContactDetails()
	                                }

	                                SmallButton {
	                                    text: "SAVE"
	                                    implicitWidth: 52
	                                    accent: root.green
	                                    enabled: !!ft2Link && contactCallText.text.trim().length > 0
	                                    tip: "Save contact details"
	                                    onClicked: root.saveContactDetails()
	                                }

	                                SmallButton {
	                                    text: "CLR"
	                                    implicitWidth: 42
	                                    accent: root.textSecondary
	                                    tip: "Clear editor"
	                                    onClicked: root.clearContactDetailsEditor()
	                                }
	                            }

		                            RowLayout {
		                                Layout.fillWidth: true
		                                Layout.fillHeight: true
		                                spacing: 6

		                                ListView {
		                                    id: contactList
		                                    Layout.preferredWidth: 430
		                                    Layout.fillHeight: true
		                                    clip: true
		                                    spacing: 2
		                                    boundsBehavior: Flickable.StopAtBounds
		                                    model: root.contactHistory
		                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

		                                    delegate: Rectangle {
		                                        id: contactDelegate
		                                        required property var modelData
		                                        width: contactList.width
		                                        height: 24
		                                        radius: 4
		                                        color: root.selectedContactCall === String(modelData.call || "")
		                                               ? root.rowSelect
		                                               : (contactMouse.containsMouse ? root.rowHover : "transparent")
		                                        border.color: root.selectedContactCall === String(modelData.call || "")
		                                                      ? root.cyan
		                                                      : "transparent"
		                                        border.width: 1

		                                        RowLayout {
		                                            anchors.fill: parent
		                                            anchors.leftMargin: 6
		                                            anchors.rightMargin: 6
		                                            spacing: 6

		                                            Text {
		                                                Layout.preferredWidth: 74
		                                                text: String(contactDelegate.modelData.call || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                font.bold: true
		                                                color: root.cyan
		                                            }

		                                            Text {
		                                                Layout.preferredWidth: 46
		                                                text: String(contactDelegate.modelData.tag || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                font.bold: text.length > 0
		                                                color: text.length > 0 ? root.amber : root.textSecondary
		                                            }

		                                            Text {
		                                                Layout.preferredWidth: 54
		                                                text: String(contactDelegate.modelData.locator || "--")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                color: root.textSecondary
		                                            }

		                                            Text {
		                                                Layout.preferredWidth: 92
		                                                text: String(contactDelegate.modelData.name || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                color: root.textPrimary
		                                            }

		                                            Text {
		                                                Layout.preferredWidth: 70
		                                                text: "q" + String(contactDelegate.modelData.qsoCount || 0)
		                                                      + " m" + String(contactDelegate.modelData.messageCount || 0)
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                color: root.green
		                                            }

		                                            Text {
		                                                Layout.fillWidth: true
		                                                text: String(contactDelegate.modelData.comment || contactDelegate.modelData.lastEvent || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                color: root.textSecondary
		                                            }
		                                        }

		                                        MouseArea {
		                                            id: contactMouse
		                                            anchors.fill: parent
		                                            hoverEnabled: true
		                                            cursorShape: Qt.PointingHandCursor
		                                            onClicked: root.loadContactDetails(contactDelegate.modelData)
		                                        }
		                                    }

		                                    Text {
		                                        anchors.centerIn: parent
		                                        visible: contactList.count === 0
		                                        text: "No contacts"
		                                        font.family: root.mono
		                                        font.pixelSize: 10
		                                        color: root.textSecondary
		                                    }
		                                }

		                                Rectangle {
		                                    Layout.preferredWidth: 1
		                                    Layout.fillHeight: true
		                                    color: root.borderSoft
		                                }

		                                ListView {
		                                    id: contactTimelineList
		                                    Layout.fillWidth: true
		                                    Layout.fillHeight: true
		                                    clip: true
		                                    spacing: 2
		                                    boundsBehavior: Flickable.StopAtBounds
		                                    model: root.selectedContactTimeline
		                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

		                                    delegate: Rectangle {
		                                        id: timelineDelegate
		                                        required property var modelData
		                                        width: contactTimelineList.width
		                                        height: 24
		                                        radius: 4
		                                        color: timelineMouse.containsMouse ? root.rowHover : "transparent"

		                                        RowLayout {
		                                            anchors.fill: parent
		                                            anchors.leftMargin: 6
		                                            anchors.rightMargin: 6
		                                            spacing: 6

		                                            Text {
		                                                Layout.preferredWidth: 48
		                                                text: String(timelineDelegate.modelData.type || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                font.bold: true
		                                                color: String(timelineDelegate.modelData.type || "") === "ALERT" ? root.red
		                                                       : (String(timelineDelegate.modelData.type || "") === "FILE" ? root.cyan
		                                                       : (String(timelineDelegate.modelData.type || "") === "MAIL" ? root.green : root.amber))
		                                            }

		                                            Text {
		                                                Layout.preferredWidth: 68
		                                                text: String(timelineDelegate.modelData.state || timelineDelegate.modelData.label || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                color: root.textSecondary
		                                            }

		                                            Text {
		                                                Layout.fillWidth: true
		                                                text: String(timelineDelegate.modelData.summary || timelineDelegate.modelData.details || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                color: root.textPrimary
		                                            }

		                                            Text {
		                                                Layout.preferredWidth: 92
		                                                text: String(timelineDelegate.modelData.details || "")
		                                                elide: Text.ElideRight
		                                                font.family: root.mono
		                                                font.pixelSize: 10
		                                                color: root.textSecondary
		                                            }
		                                        }

		                                        MouseArea {
		                                            id: timelineMouse
		                                            anchors.fill: parent
		                                            hoverEnabled: true
		                                        }
		                                    }

		                                    Text {
		                                        anchors.centerIn: parent
		                                        visible: contactTimelineList.count === 0
		                                        text: root.selectedContactCall.length > 0 ? "No contact history" : "Select contact"
		                                        font.family: root.mono
		                                        font.pixelSize: 10
		                                        color: root.textSecondary
	                            }
	                        }
	                    }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ColumnLayout {
                                Layout.preferredWidth: 320
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    spacing: 5

                                    SmallButton {
                                        text: "EXPORT"
                                        implicitWidth: 68
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Export cluster last-heard JSON"
                                        onClicked: root.exportCluster()
                                    }

                                    SmallButton {
                                        text: "IMPORT"
                                        implicitWidth: 68
                                        accent: root.amber
                                        enabled: !!ft2Link
                                        tip: "Merge pasted cluster JSON"
                                        onClicked: root.importCluster()
                                    }

                                    SmallButton {
                                        text: "COPY"
                                        implicitWidth: 54
                                        accent: root.cyan
                                        enabled: clusterJsonArea.text.length > 0
                                        tip: "Copy cluster JSON"
                                        onClicked: root.copyPlainText(clusterJsonArea.text)
                                    }

                                    SmallButton {
                                        text: "CLR"
                                        implicitWidth: 42
                                        accent: root.red
                                        enabled: !!ft2Link && root.clusterLastHeard.length > 0
                                        tip: "Clear local cluster last heard"
                                        onClicked: root.clearCluster()
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    spacing: 5

                                    TextField {
                                        id: clusterSharePathText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 24
                                        text: root.clusterSharePath
                                        placeholderText: "Shared cluster JSON path"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        selectByMouse: true
                                        onEditingFinished: root.clusterSharePath = text.trim()
                                        onAccepted: {
                                            root.clusterSharePath = text.trim()
                                            root.pullClusterShare()
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    spacing: 5

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.clusterSyncStatus.length > 0
                                              ? root.clusterSyncStatus
                                              : "Manual or auto cluster share sync"
                                        color: root.clusterSyncStatus.indexOf("error") >= 0
                                               ? root.red
                                               : root.textSecondary
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    CheckBox {
                                        text: "AUTO"
                                        checked: root.clusterAutoSync
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        onToggled: root.clusterAutoSync = checked
                                    }

                                    SmallButton {
                                        text: root.clusterAutoSyncIntervalText()
                                        implicitWidth: 44
                                        accent: root.textSecondary
                                        tip: "Cluster auto-sync interval"
                                        onClicked: root.cycleClusterAutoSyncInterval()
                                    }

                                    SmallButton {
                                        text: "SYNC"
                                        implicitWidth: 48
                                        accent: root.cyan
                                        enabled: !!ft2Link
                                        tip: "Pull, merge, and push cluster JSON"
                                        onClicked: {
                                            root.clusterSharePath = clusterSharePathText.text.trim()
                                            root.clusterLastAutoSyncMs = Date.now()
                                            root.syncClusterShare(true)
                                        }
                                    }

                                    SmallButton {
                                        text: "PUSH"
                                        implicitWidth: 48
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Write cluster JSON to shared file"
                                        onClicked: {
                                            root.clusterSharePath = clusterSharePathText.text.trim()
                                            root.pushClusterShare()
                                        }
                                    }

                                    SmallButton {
                                        text: "PULL"
                                        implicitWidth: 48
                                        accent: root.amber
                                        enabled: !!ft2Link
                                        tip: "Merge cluster JSON from shared file"
                                        onClicked: {
                                            root.clusterSharePath = clusterSharePathText.text.trim()
                                            root.pullClusterShare()
                                        }
                                    }
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 44
                                    columns: 2
                                    rowSpacing: 3
                                    columnSpacing: 6

                                    Text {
                                        text: "NODE"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.cyan
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: String(root.clusterConfigState.nodeId || "--")
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textPrimary
                                    }

                                    Text {
                                        text: "BAND"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.amber
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: String(root.clusterConfigState.band || "--")
                                              + "  "
                                              + root.frequencyHzText(Number(root.clusterConfigState.dialFrequencyHz || 0))
                                              + "  "
                                              + String(root.clusterLastHeard.length) + " rec"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }
                                }

                                ScrollView {
                                    id: clusterJsonScroll
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true
                                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

                                    TextArea {
                                        id: clusterJsonArea
                                        width: Math.max(clusterJsonScroll.availableWidth, implicitWidth)
                                        height: Math.max(clusterJsonScroll.availableHeight, implicitHeight)
                                        selectByMouse: true
                                        wrapMode: TextEdit.NoWrap
                                        placeholderText: "Paste cluster JSON here, or press EXPORT"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textPrimary
                                        selectedTextColor: root.panelBg
                                        selectionColor: root.cyan
                                        background: Rectangle {
                                            color: Qt.rgba(0.02, 0.025, 0.03, 0.90)
                                            border.color: root.borderSoft
                                            border.width: 1
                                            radius: 4
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true
                                color: root.borderSoft
                            }

                            ListView {
                                id: clusterLastHeardList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 2
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.clusterLastHeard
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: clusterDelegate
                                    required property var modelData
                                    width: clusterLastHeardList.width
                                    height: 26
                                    radius: 4
                                    color: clusterMouse.containsMouse ? root.rowHover : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 72
                                            text: String(clusterDelegate.modelData.call || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: clusterDelegate.modelData.cq ? root.green : root.cyan
                                        }

                                        Text {
                                            Layout.preferredWidth: 54
                                            text: String(clusterDelegate.modelData.band || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.amber
                                        }

                                        Text {
                                            Layout.preferredWidth: 96
                                            text: root.frequencyHzText(Number(clusterDelegate.modelData.dialFrequencyHz || 0))
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        Text {
                                            Layout.preferredWidth: 96
                                            text: String(clusterDelegate.modelData.nodeId || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        Text {
                                            Layout.preferredWidth: 58
                                            text: String(clusterDelegate.modelData.locator || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(clusterDelegate.modelData.event || "--")
                                                  + "  "
                                                  + String(clusterDelegate.modelData.source || "--")
                                                  + "  n"
                                                  + String(clusterDelegate.modelData.heardCount || 0)
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }

                                        Text {
                                            Layout.preferredWidth: 96
                                            text: String(clusterDelegate.modelData.lastHeardUtc || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }
                                    }

                                    MouseArea {
                                        id: clusterMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.NoButton
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: clusterLastHeardList.count === 0
                                    text: "No cluster last-heard records"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }
                        }
                    }
			                }
			            }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ColumnLayout {
                                Layout.preferredWidth: 260
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    spacing: 5

                                    TextField {
                                        id: pathCallText
                                        Layout.preferredWidth: 78
                                        text: root.pathFilterCall
                                        placeholderText: "CALL"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        maximumLength: 16
                                        selectByMouse: true
                                        onAccepted: root.applyPathFilter(text, pathGridText.text)
                                        onEditingFinished: root.pathFilterCall = text.trim().toUpperCase()
                                    }

                                    TextField {
                                        id: pathGridText
                                        Layout.preferredWidth: 62
                                        text: root.pathFilterGrid
                                        placeholderText: "GRID"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        maximumLength: 8
                                        selectByMouse: true
                                        onAccepted: root.applyPathFilter(pathCallText.text, text)
                                        onEditingFinished: root.pathFilterGrid = text.trim().toUpperCase()
                                    }

                                    SmallButton {
                                        text: "GO"
                                        implicitWidth: 38
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Apply path filter"
                                        onClicked: root.applyPathFilter(pathCallText.text, pathGridText.text)
                                    }

                                    SmallButton {
                                        text: "CLR"
                                        implicitWidth: 42
                                        accent: root.textSecondary
                                        tip: "Clear path filter"
                                        onClicked: {
                                            pathCallText.text = ""
                                            pathGridText.text = ""
                                            root.clearPathFilter()
                                        }
                                    }
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    columns: 2
                                    rowSpacing: 4
                                    columnSpacing: 8

                                    Text {
                                        text: "SNR"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.cyan
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: root.pathCount("snrCount") + " avg " + root.pathAverage("avgSnr")
                                              + " min " + String(root.pathValue("minSnr", 0))
                                              + " max " + String(root.pathValue("maxSnr", 0))
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textPrimary
                                    }

                                    Text {
                                        text: "BEST"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.amber
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: Number(root.pathValue("bestHourUtc", -1)) >= 0
                                              ? (root.twoDigit(root.pathValue("bestHourUtc", 0))
                                                 + "Z " + root.pathAverage("bestHourAvgSnr")
                                                 + " dB / " + root.pathCount("bestHourCount"))
                                              : "--"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textPrimary
                                    }

                                    Text {
                                        text: "PATH"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.green
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: root.pathCount("count") + " rec / " + root.statCount("pathQualityReports") + " q"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }

                                    Text {
                                        text: "BAND"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.textSecondary
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: "not tracked"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true
                                color: root.borderSoft
                            }

                            ListView {
                                id: pathReportList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 3
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.pathAnalysis && root.pathAnalysis.recentReports
                                       ? root.pathAnalysis.recentReports : []
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: pathReportDelegate
                                    required property var modelData
                                    width: pathReportList.width
                                    height: 26
                                    radius: 4
                                    color: pathReportMouse.containsMouse ? root.rowHover : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 70
                                            text: String(pathReportDelegate.modelData.remoteCall || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.textPrimary
                                        }

                                        Text {
                                            Layout.preferredWidth: 48
                                            text: String(pathReportDelegate.modelData.locator || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        Text {
                                            Layout.preferredWidth: 54
                                            text: pathReportDelegate.modelData.snrValid
                                                  ? (String(pathReportDelegate.modelData.snrDb || 0) + " dB")
                                                  : ("q " + Number(pathReportDelegate.modelData.quality || 0).toFixed(2))
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: pathReportDelegate.modelData.snrValid ? root.green : root.cyan
                                        }

                                        Text {
                                            Layout.preferredWidth: 70
                                            text: String(pathReportDelegate.modelData.direction || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }

                                        Text {
                                            Layout.preferredWidth: 64
                                            text: String(pathReportDelegate.modelData.source || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.amber
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(pathReportDelegate.modelData.atUtc || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textSecondary
                                        }
                                    }

                                    MouseArea {
                                        id: pathReportMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: pathReportList.count === 0
                                    text: "No path reports"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                        }
                    }
                }
            }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ColumnLayout {
                                Layout.preferredWidth: 224
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    SmallButton {
                                        text: "OPS"
                                        implicitWidth: 42
                                        accent: root.green
                                        tip: "Export operational log"
                                        onClicked: root.exportLog("OPS")
                                    }
                                    SmallButton {
                                        text: "ADIF"
                                        implicitWidth: 48
                                        accent: root.amber
                                        tip: "Export ADIF log"
                                        onClicked: root.exportLog("ADIF")
                                    }
                                    SmallButton {
                                        text: "OUT"
                                        implicitWidth: 46
                                        accent: root.green
                                        tip: "Export logbook outbox"
                                        onClicked: root.exportLog("OUTBOX")
                                    }
                                    SmallButton {
                                        text: "CHAT"
                                        implicitWidth: 52
                                        accent: root.cyan
                                        tip: "Export chat history"
                                        onClicked: root.exportLog("CHAT")
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    SmallButton {
                                        text: "STORE"
                                        implicitWidth: 58
                                        accent: root.textSecondary
                                        tip: "Export local store JSON"
                                        onClicked: root.exportLog("STORE")
                                    }
                                    SmallButton {
                                        text: "ALL"
                                        implicitWidth: 42
                                        accent: root.red
                                        tip: "Export all logs"
                                        onClicked: root.exportLog("BUNDLE")
                                    }
                                    SmallButton {
                                        text: "COPY"
                                        implicitWidth: 54
                                        accent: root.green
                                        enabled: root.logExportText.length > 0
                                        tip: "Copy current export"
                                        onClicked: root.copyPlainText(root.logExportText)
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    SmallButton {
                                        text: "WRITE"
                                        implicitWidth: 58
                                        accent: root.amber
                                        tip: "Write ADIF file"
                                        onClicked: root.writeAdifFile()
                                    }
                                    SmallButton {
                                        text: "PATH"
                                        implicitWidth: 48
                                        accent: root.cyan
                                        tip: "Copy ADIF file path"
                                        onClicked: root.copyAdifPath()
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    SmallButton {
                                        text: "QSO"
                                        implicitWidth: 46
                                        accent: root.green
                                        enabled: root.selectedSessionId !== 0
                                        tip: "Queue selected QSO for external logbook upload"
                                        onClicked: root.queueSelectedLogbookUpload()
                                    }
                                    SmallButton {
                                        text: "QALL"
                                        implicitWidth: 52
                                        accent: root.green
                                        enabled: root.qsoLog.length > 0
                                        tip: "Queue all FT2-Link QSOs for external logbook upload"
                                        onClicked: root.queueAllLogbookUploads()
                                    }
                                    SmallButton {
                                        text: "SEND"
                                        implicitWidth: 58
                                        accent: root.amber
                                        enabled: root.logbookOutbox.length > 0
                                        tip: "Submit queued FT2-Link ADIF records to external loggers"
                                        onClicked: root.sendQueuedLogbookUploads()
                                    }
                                    SmallButton {
                                        text: "CLR"
                                        implicitWidth: 46
                                        accent: root.red
                                        enabled: root.logbookOutbox.length > 0
                                        tip: "Clear FT2-Link logbook outbox"
                                        onClicked: root.clearLogbookOutbox()
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    text: "Outbox " + root.logbookOutbox.length
                                          + " / queued "
                                          + root.statCount("logbookQueued")
                                          + " / failed "
                                          + root.statCount("logbookFailed")
                                    wrapMode: Text.WordWrap
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            ScrollView {
                                id: logExportScroll
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                ScrollBar.horizontal.policy: ScrollBar.AsNeeded

                                TextArea {
                                    id: logExportArea
                                    width: Math.max(logExportScroll.availableWidth, implicitWidth)
                                    height: Math.max(logExportScroll.availableHeight, implicitHeight)
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: TextEdit.NoWrap
                                    text: root.logExportText
                                    placeholderText: "Select an export"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textPrimary
                                    selectedTextColor: root.panelBg
                                    selectionColor: root.cyan
                                    background: Rectangle {
                                        color: Qt.rgba(0.02, 0.025, 0.03, 0.90)
                                        border.color: root.borderSoft
                                        border.width: 1
                                        radius: 4
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ColumnLayout {
                                Layout.preferredWidth: 260
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    SmallButton {
                                        text: "AUDIT"
                                        implicitWidth: 58
                                        accent: root.cyan
                                        enabled: !!ft2Link
                                        tip: "Audit local FT2-Link store"
                                        onClicked: root.auditStore()
                                    }
                                    SmallButton {
                                        text: "BACKUP"
                                        implicitWidth: 70
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Create JSON backup"
                                        onClicked: root.backupStore()
                                    }
                                    SmallButton {
                                        text: "FIX"
                                        implicitWidth: 44
                                        accent: root.amber
                                        enabled: !!ft2Link
                                        tip: "Backup and rewrite store"
                                        onClicked: root.fixStore(true)
                                    }
                                    SmallButton {
                                        text: "SAVE"
                                        implicitWidth: 52
                                        accent: root.textSecondary
                                        enabled: !!ft2Link
                                        tip: "Rewrite store without backup"
                                        onClicked: root.fixStore(false)
                                    }
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    columns: 2
                                    rowSpacing: 4
                                    columnSpacing: 6

                                    Text {
                                        text: "STATE"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.cyan
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: String(root.storeAudit.summary || "--")
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.storeAudit.ok === false ? root.red : root.green
                                    }

                                    Text {
                                        text: "REC"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.amber
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: String(root.storeAudit.recordCount || 0)
                                              + " / " + String(root.storeAudit.serializedBytes || 0) + " B"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textPrimary
                                    }

                                    Text {
                                        text: "FILE"
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.textSecondary
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: String(root.storeAudit.storePath || "--")
                                        elide: Text.ElideMiddle
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }
                                }
                            }

                            ScrollView {
                                id: dbAuditScroll
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                ScrollBar.horizontal.policy: ScrollBar.AsNeeded

                                TextArea {
                                    id: dbAuditArea
                                    width: Math.max(dbAuditScroll.availableWidth, implicitWidth)
                                    height: Math.max(dbAuditScroll.availableHeight, implicitHeight)
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: TextEdit.NoWrap
                                    text: root.databaseActionText.length > 0
                                          ? root.databaseActionText
                                          : root.prettyJson(root.storeAudit)
                                    placeholderText: "Run audit"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textPrimary
                                    selectedTextColor: root.panelBg
                                    selectionColor: root.cyan
                                    background: Rectangle {
                                        color: Qt.rgba(0.02, 0.025, 0.03, 0.90)
                                        border.color: root.borderSoft
                                        border.width: 1
                                        radius: 4
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ListView {
                                id: statsList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 2
                                boundsBehavior: Flickable.StopAtBounds
                                model: [
                                    { label: "QSO", value: root.statCount("qsoTotal") + " / " + root.statCount("qsoDistinctCallsigns") },
                                    { label: "LONG", value: root.statCount("longestQsoMinutes") + "m / " + root.statCount("longestQsoMessages") + " msg" },
                                    { label: "CQ", value: root.statCount("cqsSent") + " tx / " + root.statCount("cqsReceived") + " rx" },
                                    { label: "BCN", value: root.statCount("beaconsSent") + " tx / " + root.statCount("beaconsReceived") + " rx" },
                                    { label: "CHAT", value: root.statCount("chatMessagesLogged") + " log / " + root.statCount("chatMessagesSent") + " tx / " + root.statCount("chatMessagesReceived") + " rx" },
                                    { label: "PING", value: root.statCount("pingsSent") + " tx / " + root.statCount("pingsReceived") + " rx / " + root.statCount("pingReplies") + " rep" },
                                    { label: "PATH", value: root.statCount("pathReportsTotal") + " rep / " + root.statCount("pathQualityReports") + " q" },
                                    { label: "CLST", value: root.statCount("clusterLastHeardTotal") + " heard" },
                                    { label: "PRE", value: root.statCount("customCannedMessages") + " custom" },
                                    { label: "ATAG", value: root.statCount("customAlertTags") + " custom" },
                                    { label: "FREQ", value: root.statCount("frequencyPresets") + " cf / " + root.statCount("allowedQsyRanges") + " rng / " + root.statCount("frequencySchedule") + " sch" },
                                    { label: "BCAST", value: root.statCount("broadcastsSent") + " tx / " + root.statCount("broadcastsReceived") + " rx" },
                                    { label: "MAIL", value: root.statCount("mailboxIncoming") + " in / " + root.statCount("mailboxOutgoing") + " out / " + root.statCount("mailboxRelay") + " relay" },
                                    { label: "FORM", value: root.statCount("formsIncoming") + " in / " + root.statCount("formsOutgoing") + " out" },
                                    { label: "FILE", value: root.statCount("filesReceived") + " rx / " + root.statCount("filesSent") + " tx / " + root.statCount("receivedFileBytes") + " B" },
                                    { label: "BBS", value: root.statCount("bulletinsIncoming") + " in / " + root.statCount("bulletinsOutgoing") + " out" },
                                    { label: "ALERT", value: root.statCount("alertsTotal") }
                                ]
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: statDelegate
                                    required property var modelData
                                    width: statsList.width
                                    height: 22
                                    radius: 4
                                    color: statMouse.containsMouse ? root.rowHover : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 54
                                            text: String(statDelegate.modelData.label || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.cyan
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(statDelegate.modelData.value || "0")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }
                                    }

                                    MouseArea {
                                        id: statMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true
                                color: root.borderSoft
                            }

                            ColumnLayout {
                                Layout.preferredWidth: 210
                                Layout.fillHeight: true
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    spacing: 6

                                    SmallButton {
                                        text: "COPY"
                                        implicitWidth: 56
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Copy statistics text"
                                        onClicked: root.copyStatisticsText()
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: "REC " + root.statCount("storeRecordsTotal")
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "LAST " + String(root.statValue("lastActivityUtc", "--"))
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.statValue("snrTracked", false)
                                          ? ("SNR rx " + root.statCount("snrsReceived")
                                             + " avg " + Number(root.statValue("snrReceivedAvg", 0)).toFixed(1)
                                             + " / tx " + root.statCount("snrsSent"))
                                          : "SNR --"
                                    elide: Text.ElideRight
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }
                        }
                    }

                    Item {
                        ListView {
                            id: receivedFileList
                            anchors.fill: parent
                            clip: true
                            spacing: 3
                            boundsBehavior: Flickable.StopAtBounds
                            model: root.receivedFiles
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            delegate: Rectangle {
                                id: rxFileDelegate
                                required property var modelData
                                width: receivedFileList.width
                                height: 28
                                radius: 4
                                color: rxFileMouse.containsMouse ? root.rowHover : "transparent"

                                MouseArea {
                                    id: rxFileMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    anchors.rightMargin: 6
                                    spacing: 6

                                    Text {
                                        Layout.preferredWidth: 42
                                        text: rxFileDelegate.modelData.imageLike ? "IMG" : "FILE"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: rxFileDelegate.modelData.imageLike ? root.amber : root.cyan
                                    }

                                    Text {
                                        Layout.preferredWidth: 76
                                        text: String(rxFileDelegate.modelData.senderCall || "--")
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: root.textPrimary
                                    }

                                    Text {
                                        Layout.preferredWidth: 150
                                        text: String(rxFileDelegate.modelData.fileName || "")
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.green
                                    }

                                    Text {
                                        Layout.preferredWidth: 104
                                        text: root.receivedFileDate(rxFileDelegate.modelData)
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }

                                    Text {
                                        Layout.preferredWidth: 66
                                        text: String(rxFileDelegate.modelData.sizeBytes || 0) + " B"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: String(rxFileDelegate.modelData.preview || "")
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textPrimary
                                    }

                                    SmallButton {
                                        text: "COPY"
                                        implicitWidth: 52
                                        accent: root.cyan
                                        enabled: String(rxFileDelegate.modelData.content || "").length > 0
                                        tip: "Copy received file content"
                                        onClicked: {
                                            var text = String(rxFileDelegate.modelData.content || "")
                                            if (bridge && typeof bridge.copyToClipboard === "function")
                                                bridge.copyToClipboard(text)
                                            else
                                                composeText.text = text
                                        }
                                    }
                                }
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: receivedFileList.count === 0
                                text: "No received files"
                                font.family: root.mono
                                font.pixelSize: 10
                                color: root.textSecondary
                            }
                        }
                    }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ColumnLayout {
                                Layout.preferredWidth: 330
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    TextField {
                                        id: presetLabelText
                                        Layout.preferredWidth: 82
                                        Layout.preferredHeight: 26
                                        placeholderText: "LABEL"
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        maximumLength: 12
                                        selectByMouse: true
                                    }

                                    TextField {
                                        id: presetTipText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 26
                                        placeholderText: "description"
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        maximumLength: 96
                                        selectByMouse: true
                                    }
                                }

                                TextField {
                                    id: presetTemplateText
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    placeholderText: "template text with <MYCALL>, <CALL>, <QTH>..."
                                    font.family: root.mono
                                    font.pixelSize: 11
                                    maximumLength: 512
                                    selectByMouse: true
                                    onAccepted: root.savePreset()
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    SmallButton {
                                        text: "SAVE"
                                        implicitWidth: 52
                                        accent: root.green
                                        enabled: presetLabelText.text.trim().length > 0
                                                 && presetTemplateText.text.trim().length > 0
                                        tip: "Save custom preset"
                                        onClicked: root.savePreset()
                                    }

                                    SmallButton {
                                        text: "DEL"
                                        implicitWidth: 44
                                        accent: root.red
                                        enabled: presetLabelText.text.trim().length > 0
                                        tip: "Delete custom preset"
                                        onClicked: root.deletePreset()
                                    }

                                    SmallButton {
                                        text: "RST"
                                        implicitWidth: 44
                                        accent: root.amber
                                        enabled: root.customCannedMessages.length > 0
                                        tip: "Clear all custom presets"
                                        onClicked: root.resetPresets()
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: String(root.customCannedMessages.length) + " custom"
                                        elide: Text.ElideRight
                                        font.family: root.mono
                                        font.pixelSize: 10
                                        color: root.textSecondary
                                    }
                                }
                            }

                            ListView {
                                id: customPresetList
                                Layout.preferredWidth: 210
                                Layout.fillHeight: true
                                clip: true
                                spacing: 3
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.customCannedMessages
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: presetDelegate
                                    required property var modelData
                                    width: customPresetList.width
                                    height: 26
                                    radius: 4
                                    color: presetMouse.containsMouse ? root.rowHover : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 58
                                            text: String(presetDelegate.modelData.label || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.amber
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(presetDelegate.modelData.templateText || "")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }
                                    }

                                    MouseArea {
                                        id: presetMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: root.loadPreset(presetDelegate.modelData)
                                        onDoubleClicked: root.insertCannedMessage(String(presetDelegate.modelData.templateText || ""))
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: customPresetList.count === 0
                                    text: "No custom presets"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    TextField {
                                        id: checkInCityText
                                        Layout.preferredWidth: 120
                                        Layout.preferredHeight: 26
                                        text: root.checkInCity
                                        placeholderText: root.profileQth.length > 0 ? root.profileQth : "city"
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                    }

                                    TextField {
                                        id: checkInRegionText
                                        Layout.preferredWidth: 120
                                        Layout.preferredHeight: 26
                                        text: root.checkInRegion
                                        placeholderText: "county/state"
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                    }

                                    TextField {
                                        id: checkInChannelText
                                        Layout.preferredWidth: 54
                                        Layout.preferredHeight: 26
                                        text: root.checkInChannel
                                        placeholderText: "HF"
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        maximumLength: 8
                                        selectByMouse: true
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    TextField {
                                        id: checkInWeatherText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        placeholderText: "weather line optional"
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                    }

                                    SmallButton {
                                        text: "MAIL"
                                        implicitWidth: 54
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Prepare VarAC Wednesday mail"
                                        onClicked: root.prepareCheckInMail()
                                    }

                                    SmallButton {
                                        text: "CHAT"
                                        implicitWidth: 54
                                        accent: root.cyan
                                        enabled: !!ft2Link
                                        tip: "Insert check-in in chat composer"
                                        onClicked: root.prepareCheckInChat()
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    text: "Wednesday check-in: MAIL sets address, subject and body; CHAT inserts only the body."
                                    wrapMode: Text.WordWrap
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }
                        }
                    }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 30
                                    spacing: 5

                                    TextField {
                                        id: frequencyPresetText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        text: ft2Link && typeof ft2Link.frequencyPresetsText === "function"
                                              ? ft2Link.frequencyPresetsText()
                                              : ""
                                        placeholderText: "14105000|20m|Main, 7105000|40m|Main"
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                        onAccepted: root.saveFrequencyPresets()
                                    }

                                    SmallButton {
                                        text: "SAVE"
                                        implicitWidth: 52
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Save calling frequency presets"
                                        onClicked: root.saveFrequencyPresets()
                                    }

                                    SmallButton {
                                        text: "RST"
                                        implicitWidth: 44
                                        accent: root.amber
                                        enabled: !!ft2Link
                                        tip: "Restore default frequency presets"
                                        onClicked: root.resetFrequencyPresets()
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 30
                                    spacing: 5

                                    TextField {
                                        id: frequencyScheduleText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        text: ft2Link && typeof ft2Link.frequencyScheduleText === "function"
                                              ? ft2Link.frequencyScheduleText()
                                              : ""
                                        placeholderText: "0000-2359|CALLING|14105000|20m main|CQ"
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                        onAccepted: root.saveFrequencySchedule()
                                    }

                                    SmallButton {
                                        text: "SAVE"
                                        implicitWidth: 52
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Save UTC frequency schedule"
                                        onClicked: root.saveFrequencySchedule()
                                    }

                                    SmallButton {
                                        text: "CLR"
                                        implicitWidth: 44
                                        accent: root.amber
                                        enabled: !!ft2Link
                                        tip: "Clear frequency schedule"
                                        onClicked: root.resetFrequencySchedule()
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 30
                                    spacing: 5

                                    TextField {
                                        id: allowedQsyRangeText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        text: ft2Link && typeof ft2Link.allowedQsyRangesText === "function"
                                              ? ft2Link.allowedQsyRangesText()
                                              : ""
                                        placeholderText: "14101250-14108750|20m, 7101250-7108750|40m"
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                        onAccepted: root.saveAllowedQsyRanges()
                                    }

                                    SmallButton {
                                        text: "SAVE"
                                        implicitWidth: 52
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Save allowed QSY ranges"
                                        onClicked: root.saveAllowedQsyRanges()
                                    }

                                    SmallButton {
                                        text: "RST"
                                        implicitWidth: 44
                                        accent: root.amber
                                        enabled: !!ft2Link
                                        tip: "Restore default QSY ranges"
                                        onClicked: root.resetAllowedQsyRanges()
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    text: "FREQ stores calling-frequency presets, UTC schedule windows and allowed QSY ranges. Schedule actions CALLING/CQ/BEACON/EMCOMM/QUIET protect the active frequency; DATA marks a data window. CAT auto-QSY is not performed."
                                    wrapMode: Text.WordWrap
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true
                                color: root.borderSoft
                            }

                            ListView {
                                id: frequencyPresetListView
                                Layout.preferredWidth: 230
                                Layout.fillHeight: true
                                clip: true
                                spacing: 3
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.frequencyPresetList
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: freqDelegate
                                    required property var modelData
                                    width: frequencyPresetListView.width
                                    height: 24
                                    radius: 4
                                    color: freqMouse.containsMouse ? root.rowHover : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 58
                                            text: String(freqDelegate.modelData.band || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.cyan
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: root.frequencyHzText(Number(freqDelegate.modelData.dialFrequencyHz || 0))
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }
                                    }

                                    MouseArea {
                                        id: freqMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }
                                }
                            }

                            ListView {
                                id: allowedRangeListView
                                Layout.preferredWidth: 250
                                Layout.fillHeight: true
                                clip: true
                                spacing: 3
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.allowedQsyRangeList
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: rangeDelegate
                                    required property var modelData
                                    width: allowedRangeListView.width
                                    height: 24
                                    radius: 4
                                    color: rangeMouse.containsMouse ? root.rowHover : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 48
                                            text: String(rangeDelegate.modelData.label || "--")
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.amber
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: root.frequencyHzText(Number(rangeDelegate.modelData.fromHz || 0))
                                                  + " - "
                                                  + root.frequencyHzText(Number(rangeDelegate.modelData.toHz || 0))
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            color: root.textPrimary
                                        }
                                    }

                                    MouseArea {
                                        id: rangeMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        RowLayout {
                            anchors.fill: parent
                            spacing: 6

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 30
                                    spacing: 5

                                    TextField {
                                        id: blockedCallsText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        text: ft2Link && typeof ft2Link.blockedCallsText === "function"
                                              ? ft2Link.blockedCallsText()
                                              : ""
                                        placeholderText: "CALL1, CALL2, Z6/TEST"
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        selectByMouse: true
                                        onAccepted: root.saveBlockedCalls()
                                    }

                                    SmallButton {
                                        text: "SAVE"
                                        implicitWidth: 52
                                        accent: root.green
                                        enabled: !!ft2Link
                                        tip: "Save blocked callsigns"
                                        onClicked: root.saveBlockedCalls()
                                    }

                                    SmallButton {
                                        text: "CLR"
                                        implicitWidth: 42
                                        accent: root.red
                                        enabled: !!ft2Link && root.blockedCalls.length > 0
                                        tip: "Clear blocked callsigns"
                                        onClicked: root.clearBlockedCalls()
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 30
                                    spacing: 5

                                    TextField {
                                        id: blockedCallText
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        placeholderText: "Add callsign"
                                        font.family: root.mono
                                        font.pixelSize: 11
                                        maximumLength: 24
                                        selectByMouse: true
                                        onAccepted: root.addBlockedCallFromEditor()
                                    }

                                    SmallButton {
                                        text: "ADD"
                                        implicitWidth: 48
                                        accent: root.amber
                                        enabled: !!ft2Link && blockedCallText.text.trim().length > 0
                                        tip: "Add callsign to block list"
                                        onClicked: root.addBlockedCallFromEditor()
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    text: "Blocked calls cannot start a session, are hidden from last-heard, and their beacon, CQ, ping and broadcast traffic is ignored locally."
                                    wrapMode: Text.WordWrap
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true
                                color: root.borderSoft
                            }

                            ListView {
                                id: blockedCallListView
                                Layout.preferredWidth: 260
                                Layout.fillHeight: true
                                clip: true
                                spacing: 3
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.blockedCalls
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Rectangle {
                                    id: blockedCallDelegate
                                    required property string modelData
                                    width: blockedCallListView.width
                                    height: 24
                                    radius: 4
                                    color: blockedMouse.containsMouse ? root.rowHover : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 6

                                        Text {
                                            Layout.fillWidth: true
                                            text: blockedCallDelegate.modelData
                                            elide: Text.ElideRight
                                            font.family: root.mono
                                            font.pixelSize: 10
                                            font.bold: true
                                            color: root.red
                                        }

                                        SmallButton {
                                            text: "DEL"
                                            implicitWidth: 42
                                            accent: root.red
                                            enabled: !!ft2Link
                                            tip: "Remove callsign from block list"
                                            onClicked: root.deleteBlockedCall(blockedCallDelegate.modelData)
                                        }
                                    }

                                    MouseArea {
                                        id: blockedMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.NoButton
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: blockedCallListView.count === 0
                                    text: "No blocked calls"
                                    font.family: root.mono
                                    font.pixelSize: 10
                                    color: root.textSecondary
                                }
                            }
                        }
                    }
		                }
		            }
		        }
        }
    }
}
