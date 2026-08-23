// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/core/SstvModeRegistry.h"

#include <array>
#include <set>
#include <string>
#include <vector>

using namespace decodium::sstv;

namespace {

SstvModeSpec completeImplementedMode(const char* id,
                                     const char* longName,
                                     const char* shortName,
                                     std::uint8_t visCode)
{
    SstvModeSpec mode;
    mode.id = id;
    mode.longName = longName;
    mode.shortName = shortName;
    mode.family = "Test family";
    mode.classification = ModeClassification::AnalogSstv;
    mode.catalogStatus = CatalogStatus::Catalogued;
    mode.rxStatus = CapabilityStatus::Implemented;
    mode.protocolDataComplete = true;

    SstvVisSpec vis;
    vis.encoding = VisEncoding::StandardSevenBit;
    vis.bitCount = 7;
    vis.standardCode = visCode;
    vis.lsbFirst = true;
    vis.parity = Parity::Even;
    mode.vis = vis;

    mode.geometry.imageWidth = 320;
    mode.geometry.imageHeight = 256;
    mode.geometry.transmittedLineCount = 256;
    mode.geometry.displayedLineCount = 256;
    mode.geometry.linesPerScan = 1;

    mode.colour.colourSpace = ColourSpace::Grayscale;
    mode.colour.componentOrder = {ColourComponent::Gray};
    mode.colour.chromaSubsampling = ChromaSubsampling::NotApplicable;
    mode.colour.conversionRule = "full-range grayscale test rule";

    mode.timing.syncFrequencyHz = 1200;
    mode.timing.syncDuration = Picoseconds {5 * kPicosecondsPerMillisecond};
    mode.timing.frontPorch = Picoseconds {0};
    mode.timing.backPorch = Picoseconds {0};
    mode.timing.separatorFrequencyHz = 0;
    mode.timing.separatorDuration = Picoseconds {0};
    mode.timing.pixelDuration = Picoseconds {kPicosecondsPerMillisecond};
    mode.timing.lineDuration = Picoseconds {100 * kPicosecondsPerMillisecond};
    mode.timing.imageDuration = Picoseconds {10 * kPicosecondsPerSecond};
    mode.timing.nominalAudioBandwidth = SstvAudioBandwidth {1100, 2500};
    mode.timing.tolerancePpm = 1000;

    mode.leaderHeaderRules = "deterministic unit-test header";
    mode.specialLineOrdering = "none";
    mode.fallbackSignature.discriminator = "no fallback in unit test";
    mode.protocolProvenance = {"unit-test protocol specification"};
    mode.evidenceStatus = EvidenceStatus::DeterministicTests;
    mode.implementationEvidenceRefs = {"test_sstv_mode_registry"};
    mode.statusNote = "Synthetic validator fixture; not a canonical mode.";
    return mode;
}

} // namespace

class TestSstvModeRegistry final : public QObject
{
    Q_OBJECT

private slots:
    void canonicalRegistryIsValidAndCompleteAsCatalogue()
    {
        const auto registry = SstvModeRegistry::canonical();
        QCOMPARE(registry.modes().size(), std::size_t {55});
        QVERIFY2(registry.isValid(), "canonical SSTV catalogue must pass structural validation");

        std::set<std::string> ids;
        std::set<std::string> longNames;
        std::set<std::string> shortNames;
        for (const auto& mode : registry.modes()) {
            QVERIFY(!mode.id.empty());
            QVERIFY(!mode.longName.empty());
            QVERIFY(!mode.shortName.empty());
            QVERIFY(!mode.family.empty());
            QVERIFY(ids.insert(mode.id).second);
            QVERIFY(longNames.insert(mode.longName).second);
            QVERIFY(shortNames.insert(mode.shortName).second);

            // Discovery entries deliberately do not claim implementation.
            QVERIFY(!mode.protocolDataComplete);
            QVERIFY(!mode.claimsRxSupport());
            QVERIFY(!mode.claimsTxSupport());
            QVERIFY(!mode.hasImplementationEvidence());
            QVERIFY(!mode.catalogueReferences.empty());
        }
    }

    void canonicalRegistryContainsEveryMandatoryIdentity()
    {
        const auto registry = SstvModeRegistry::canonical();
        const std::array<const char*, 55> required {{
            "martin-m1", "martin-m2", "martin-m3", "martin-m4",
            "scottie-s1", "scottie-s2", "scottie-dx", "scottie-s3", "scottie-s4",
            "robot-c12", "robot-c24", "robot-c36", "robot-c72",
            "robot-bw8", "robot-bw12", "robot-bw24", "robot-bw36",
            "wraase-sc2-60", "wraase-sc2-120", "wraase-sc2-180",
            "pasokon-p3", "pasokon-p5", "pasokon-p7",
            "pd-50", "pd-90", "pd-120", "pd-160", "pd-180", "pd-240", "pd-290",
            "avt-24", "avt-90", "avt-94",
            "mp-73", "mp-115", "mp-140", "mp-175",
            "mr-73", "mr-90", "mr-115", "mr-140", "mr-175",
            "ml-180", "ml-240", "ml-280", "ml-320",
            "mp-73-narrow", "mp-110-narrow", "mp-140-narrow",
            "mc-110-narrow", "mc-140-narrow", "mc-180-narrow",
            "fax-480", "hffax", "wefax"
        }};

        for (const auto id : required) {
            QVERIFY2(registry.findById(id) != nullptr, id);
        }

        QCOMPARE(registry.findByName("Martin M1"), registry.findById("martin-m1"));
        QCOMPARE(registry.findByName("M1"), registry.findById("martin-m1"));
        QVERIFY(registry.findById("not-a-mode") == nullptr);
        QVERIFY(registry.findByName("not-a-mode") == nullptr);
    }

    void relatedFaxModesAreNotMisclassifiedAsAnalogSstv()
    {
        const auto registry = SstvModeRegistry::canonical();
        for (const auto id : {"fax-480", "hffax", "wefax"}) {
            const auto* mode = registry.findById(id);
            QVERIFY(mode != nullptr);
            QVERIFY(mode->classification == ModeClassification::RelatedFax);
            QCOMPARE(QString::fromStdString(mode->family), QStringLiteral("FAX"));
        }
        QVERIFY(registry.findById("martin-m1")->classification
                == ModeClassification::AnalogSstv);
    }

    void auditedConflictsRemainExplicitlyBlocked()
    {
        const auto registry = SstvModeRegistry::canonical();
        const std::array<const char*, 13> blocked {{
            "martin-m1", "martin-m2", "martin-m3", "scottie-s2",
            "robot-c24", "robot-bw8", "robot-bw12", "robot-bw24", "robot-bw36",
            "wraase-sc2-120", "mr-140", "mr-175", "fax-480"
        }};

        for (const auto id : blocked) {
            const auto* mode = registry.findById(id);
            QVERIFY2(mode != nullptr, id);
            QVERIFY2(mode->catalogStatus == CatalogStatus::Blocked, id);
            QVERIFY2(mode->rxStatus == CapabilityStatus::Blocked, id);
            QVERIFY2(mode->txStatus == CapabilityStatus::Blocked, id);
            QVERIFY2(!mode->statusNote.empty(), id);
        }

        const auto* mr140 = registry.findById("mr-140");
        const auto* mr175 = registry.findById("mr-175");
        QVERIFY(mr140->statusNote.find("0x4A23") != std::string::npos);
        QVERIFY(mr175->statusNote.find("0x4A23") != std::string::npos);
        QVERIFY(mr175->statusNote.find("0x4C23") != std::string::npos);
        QVERIFY(!mr140->vis.has_value());
        QVERIFY(!mr175->vis.has_value());
    }

    void incompleteCatalogueRowsMayOmitUnknownProtocolValues()
    {
        SstvModeSpec discovery;
        discovery.id = "discovery-only";
        discovery.longName = "Discovery only";
        discovery.shortName = "DISC";
        discovery.family = "Test";
        discovery.statusNote = "No protocol data claimed.";

        auto issues = SstvModeRegistry::validate({discovery});
        QVERIFY(issues.empty());

        discovery.protocolDataComplete = true;
        issues = SstvModeRegistry::validate({discovery});
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::MissingProtocolField));
    }

    void duplicateIdsAndNamesAreRejected()
    {
        auto first = completeImplementedMode("one", "First mode", "ONE", 1);
        auto duplicate = completeImplementedMode("one", "First mode", "ONE", 2);
        const auto issues = SstvModeRegistry::validate({first, duplicate});
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::DuplicateId));
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::DuplicateLongName));
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::DuplicateShortName));
    }

    void supportClaimsRequireCompleteDataAndExecutableEvidence()
    {
        SstvModeSpec unsupported;
        unsupported.id = "unsupported-claim";
        unsupported.longName = "Unsupported claim";
        unsupported.shortName = "UNSUP";
        unsupported.family = "Test";
        unsupported.rxStatus = CapabilityStatus::Implemented;

        auto issues = SstvModeRegistry::validate({unsupported});
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::CapabilityWithoutProtocolData));
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::CapabilityWithoutEvidence));

        auto complete = completeImplementedMode("complete", "Complete mode", "COMP", 3);
        issues = SstvModeRegistry::validate({complete});
        QVERIFY2(issues.empty(), "implemented status with complete data and deterministic evidence must validate");

        complete.evidenceStatus = EvidenceStatus::AuditedSources;
        complete.implementationEvidenceRefs.clear();
        issues = SstvModeRegistry::validate({complete});
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::CapabilityWithoutEvidence));
    }

    void verifiedClaimsRequireIndependentEvidence()
    {
        auto mode = completeImplementedMode("verified", "Verified mode", "VER", 4);
        mode.catalogStatus = CatalogStatus::Verified;
        mode.rxStatus = CapabilityStatus::Verified;

        auto issues = SstvModeRegistry::validate({mode});
        QVERIFY(containsValidationIssue(issues,
                                        ModeValidationCode::VerifiedWithoutIndependentEvidence));

        mode.evidenceStatus = EvidenceStatus::IndependentVector;
        mode.interoperabilityStatus = InteroperabilityStatus::IndependentlyVerified;
        mode.fixtureStatus = FixtureStatus::Independent;
        mode.implementationEvidenceRefs.push_back("independent vector hash and test result");
        issues = SstvModeRegistry::validate({mode});
        QVERIFY(issues.empty());
    }

    void duplicateVisRequiresAnExplicitSharedCodeGroup()
    {
        auto first = completeImplementedMode("vis-one", "VIS one", "VIS1", 5);
        auto second = completeImplementedMode("vis-two", "VIS two", "VIS2", 5);

        auto issues = SstvModeRegistry::validate({first, second});
        QVERIFY(containsValidationIssue(issues, ModeValidationCode::DuplicateVisCode));

        first.vis->documentedSharedCodeGroup = "documented-alias-test";
        second.vis->documentedSharedCodeGroup = "documented-alias-test";
        issues = SstvModeRegistry::validate({first, second});
        QVERIFY(!containsValidationIssue(issues, ModeValidationCode::DuplicateVisCode));
    }
};

QTEST_APPLESS_MAIN(TestSstvModeRegistry)
#include "test_sstv_mode_registry.moc"
