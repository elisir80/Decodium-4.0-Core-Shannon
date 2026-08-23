// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvModeRegistry.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace decodium::sstv {
namespace {

struct CatalogueSeed final
{
    const char* id;
    const char* longName;
    const char* shortName;
    const char* family;
    ModeClassification classification;
    CatalogStatus status;
    const char* note;
};

constexpr const char* kCatalogueReference = "Mission section 5.1 and docs/sstv/MODE_CATALOG.md";

SstvModeSpec fromSeed(const CatalogueSeed& seed)
{
    SstvModeSpec mode;
    mode.id = seed.id;
    mode.longName = seed.longName;
    mode.shortName = seed.shortName;
    mode.family = seed.family;
    mode.classification = seed.classification;
    mode.catalogStatus = seed.status;
    mode.statusNote = seed.note;
    mode.catalogueReferences.emplace_back(kCatalogueReference);

    if (seed.status == CatalogStatus::Blocked) {
        mode.rxStatus = CapabilityStatus::Blocked;
        mode.txStatus = CapabilityStatus::Blocked;
        mode.autoDetectStatus = CapabilityStatus::Blocked;
        mode.interoperabilityStatus = InteroperabilityStatus::Blocked;
        mode.fixtureStatus = FixtureStatus::Blocked;
    }
    return mode;
}

void addIssue(std::vector<ModeValidationIssue>& issues,
              ModeValidationCode code,
              const SstvModeSpec& mode,
              std::string message)
{
    issues.push_back({code, mode.id, std::move(message)});
}

template<typename T>
bool presentAndPositive(const std::optional<T>& value)
{
    return value.has_value() && *value > 0;
}

bool presentAndPositive(const std::optional<Picoseconds>& value)
{
    return value.has_value() && value->count > 0;
}

bool isNegative(const std::optional<Picoseconds>& value)
{
    return value.has_value() && value->count < 0;
}

bool claimsCapability(CapabilityStatus status) noexcept
{
    return status == CapabilityStatus::Implemented || status == CapabilityStatus::Verified;
}

bool hasVerifiedCapability(const SstvModeSpec& mode) noexcept
{
    return mode.rxStatus == CapabilityStatus::Verified
        || mode.txStatus == CapabilityStatus::Verified
        || mode.autoDetectStatus == CapabilityStatus::Verified;
}

std::string standardVisKey(std::uint8_t code)
{
    return std::string("standard:") + std::to_string(code);
}

std::string extendedVisKey(const std::vector<std::uint8_t>& bytes)
{
    std::ostringstream stream;
    stream << "extended:";
    for (const auto byte : bytes) {
        stream << static_cast<unsigned int>(byte) << ',';
    }
    return stream.str();
}

void validateProtocolFields(const SstvModeSpec& mode,
                            std::vector<ModeValidationIssue>& issues)
{
    const std::array<std::optional<Picoseconds>, 8> durations {{
        mode.timing.syncDuration,
        mode.timing.frontPorch,
        mode.timing.backPorch,
        mode.timing.separatorDuration,
        mode.timing.pixelDuration,
        mode.timing.componentDuration,
        mode.timing.lineDuration,
        mode.timing.imageDuration
    }};
    if (std::any_of(durations.begin(), durations.end(), isNegative)) {
        addIssue(issues, ModeValidationCode::InvalidProtocolValue, mode,
                 "timing values must not be negative");
    }

    if (mode.timing.syncFrequencyHz.has_value() && *mode.timing.syncFrequencyHz == 0U) {
        addIssue(issues, ModeValidationCode::InvalidProtocolValue, mode,
                 "a specified sync frequency must be positive");
    }
    if (mode.timing.separatorDuration.has_value()
        && mode.timing.separatorDuration->count > 0
        && !presentAndPositive(mode.timing.separatorFrequencyHz)) {
        addIssue(issues, ModeValidationCode::InvalidProtocolValue, mode,
                 "a positive separator duration requires a positive frequency");
    }
    if (mode.timing.nominalAudioBandwidth.has_value()) {
        const auto& bandwidth = *mode.timing.nominalAudioBandwidth;
        if (bandwidth.lowHz == 0U || bandwidth.highHz <= bandwidth.lowHz) {
            addIssue(issues, ModeValidationCode::InvalidProtocolValue, mode,
                     "nominal audio bandwidth must have positive ordered limits");
        }
    }

    if (!mode.protocolDataComplete) {
        return;
    }

    const bool geometryComplete = presentAndPositive(mode.geometry.imageWidth)
        && presentAndPositive(mode.geometry.imageHeight)
        && presentAndPositive(mode.geometry.transmittedLineCount)
        && presentAndPositive(mode.geometry.displayedLineCount)
        && presentAndPositive(mode.geometry.linesPerScan);
    if (!geometryComplete) {
        addIssue(issues, ModeValidationCode::MissingProtocolField, mode,
                 "complete protocol data requires image and line geometry");
    }

    if (!mode.vis.has_value()) {
        addIssue(issues, ModeValidationCode::MissingProtocolField, mode,
                 "complete protocol data requires an explicit VIS rule, including no-VIS");
    }

    const bool colourComplete = mode.colour.colourSpace != ColourSpace::Unknown
        && !mode.colour.componentOrder.empty()
        && mode.colour.chromaSubsampling != ChromaSubsampling::Unknown
        && !mode.colour.conversionRule.empty();
    if (!colourComplete) {
        addIssue(issues, ModeValidationCode::MissingProtocolField, mode,
                 "complete protocol data requires colour order, conversion and subsampling");
    }

    const bool timingComplete = presentAndPositive(mode.timing.syncFrequencyHz)
        && presentAndPositive(mode.timing.syncDuration)
        && mode.timing.frontPorch.has_value()
        && mode.timing.backPorch.has_value()
        && mode.timing.separatorFrequencyHz.has_value()
        && mode.timing.separatorDuration.has_value()
        && (presentAndPositive(mode.timing.pixelDuration)
            || presentAndPositive(mode.timing.componentDuration))
        && presentAndPositive(mode.timing.lineDuration)
        && presentAndPositive(mode.timing.imageDuration)
        && mode.timing.nominalAudioBandwidth.has_value()
        && presentAndPositive(mode.timing.tolerancePpm);
    if (!timingComplete) {
        addIssue(issues, ModeValidationCode::MissingProtocolField, mode,
                 "complete protocol data requires every timing and bandwidth field");
    }

    if (mode.leaderHeaderRules.empty() || mode.specialLineOrdering.empty()) {
        addIssue(issues, ModeValidationCode::MissingProtocolField, mode,
                 "complete protocol data requires header and line-order rules");
    }
    if (mode.fallbackSignature.discriminator.empty()) {
        addIssue(issues, ModeValidationCode::MissingProtocolField, mode,
                 "complete protocol data requires an explicit fallback rule or no-fallback marker");
    }
    if (mode.protocolProvenance.empty()) {
        addIssue(issues, ModeValidationCode::MissingProtocolField, mode,
                 "complete protocol data requires authoritative provenance");
    }
}

void validateVis(const SstvModeSpec& mode, std::vector<ModeValidationIssue>& issues)
{
    if (!mode.vis.has_value()) {
        return;
    }
    const auto& vis = *mode.vis;
    bool valid = true;
    switch (vis.encoding) {
    case VisEncoding::None:
        valid = vis.bitCount == 0U && !vis.standardCode.has_value()
            && vis.standardAliases.empty() && vis.extendedSequence.empty()
            && vis.parity == Parity::None;
        break;
    case VisEncoding::StandardSevenBit:
        valid = vis.bitCount == 7U && vis.standardCode.has_value()
            && *vis.standardCode < 128U && vis.extendedSequence.empty()
            && vis.lsbFirst && vis.parity == Parity::Even;
        valid = valid && std::all_of(vis.standardAliases.begin(), vis.standardAliases.end(),
                                    [](std::uint8_t code) { return code < 128U; });
        break;
    case VisEncoding::Extended:
        valid = vis.bitCount > 7U && !vis.standardCode.has_value()
            && vis.standardAliases.empty() && !vis.extendedSequence.empty();
        break;
    case VisEncoding::Unknown:
        valid = false;
        break;
    }
    if (!valid) {
        addIssue(issues, ModeValidationCode::InvalidVis, mode,
                 "VIS encoding fields are internally inconsistent");
    }
}

void validateCapability(const SstvModeSpec& mode,
                        CapabilityStatus status,
                        const char* direction,
                        std::vector<ModeValidationIssue>& issues)
{
    if (!claimsCapability(status)) {
        return;
    }
    if (!mode.protocolDataComplete) {
        addIssue(issues, ModeValidationCode::CapabilityWithoutProtocolData, mode,
                 std::string(direction) + " capability requires complete protocol data");
    }
    if (!mode.hasImplementationEvidence() || mode.implementationEvidenceRefs.empty()) {
        addIssue(issues, ModeValidationCode::CapabilityWithoutEvidence, mode,
                 std::string(direction) + " capability requires executable implementation evidence");
    }
    if (status == CapabilityStatus::Verified) {
        if (mode.catalogStatus != CatalogStatus::Verified) {
            addIssue(issues, ModeValidationCode::InconsistentStatus, mode,
                     std::string(direction) + " verified capability requires verified catalogue state");
        }
        if (!mode.hasIndependentEvidence()) {
            addIssue(issues, ModeValidationCode::VerifiedWithoutIndependentEvidence, mode,
                     std::string(direction) + " verification requires an independent fixture and interoperability result");
        }
    }
}

} // namespace

SstvModeRegistry::SstvModeRegistry(std::vector<SstvModeSpec> modes)
    : m_modes(std::move(modes))
{
}

SstvModeRegistry SstvModeRegistry::canonical()
{
    // These are catalogue identities only.  No timing, dimensions or VIS are
    // populated until the audit conflicts and independent-evidence gates are
    // resolved.  This prevents a discovery list from becoming a support claim.
    const CatalogueSeed seeds[] = {
        {"martin-m1", "Martin M1", "M1", "Martin", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: audited sources disagree on 5.000 ms versus 4.862 ms sync timing."},
        {"martin-m2", "Martin M2", "M2", "Martin", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: audited sources disagree on 160 versus 320 pixel geometry."},
        {"martin-m3", "Martin M3", "M3", "Martin", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: the audited SlowRX pixel timing conflicts with its own line duration and libsstv."},
        {"martin-m4", "Martin M4", "M4", "Martin", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; an independent authoritative specification and vector are still required."},

        {"scottie-s1", "Scottie S1", "S1", "Scottie", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; first-line/sync ordering and an independent vector remain to be verified."},
        {"scottie-s2", "Scottie S2", "S2", "Scottie", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: audited sources disagree on 160 versus 320 pixel geometry."},
        {"scottie-dx", "Scottie DX", "SDX", "Scottie", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent long-duration evidence is still required."},
        {"scottie-s3", "Scottie S3", "S3", "Scottie", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued from one audited encoder path; independent specification/vector missing."},
        {"scottie-s4", "Scottie S4", "S4", "Scottie", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued from one audited encoder path; independent specification/vector missing."},

        {"robot-c12", "Robot 12 Colour", "R12C", "Robot colour", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; historical geometry, VIS and line-pair behaviour need independent evidence."},
        {"robot-c24", "Robot 24 Colour", "R24C", "Robot colour", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: audited sources report conflicting 160x120, 320x120 and 320x240 geometries."},
        {"robot-c36", "Robot 36 Colour", "R36C", "Robot colour", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; alternating chroma and displayed/transmitted rows need independent tests."},
        {"robot-c72", "Robot 72 Colour", "R72C", "Robot colour", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; line-pair chroma needs independent tests."},

        {"robot-bw8", "Robot B/W 8", "RBW8", "Robot monochrome", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: corrected timing and conflicting R/G/B VIS aliases require authoritative resolution."},
        {"robot-bw12", "Robot B/W 12", "RBW12", "Robot monochrome", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: audited sources disagree on the applicable R/G/B VIS aliases."},
        {"robot-bw24", "Robot B/W 24", "RBW24", "Robot monochrome", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: audited sources disagree on the applicable R/G/B VIS aliases."},
        {"robot-bw36", "Robot B/W 36", "RBW36", "Robot monochrome", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: audited sources disagree on the applicable R/G/B VIS aliases and no decoder vector exists."},

        {"wraase-sc2-60", "Wraase SC2-60", "SC2-60", "Wraase", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; only one audited implementation lineage and no independent vector."},
        {"wraase-sc2-120", "Wraase SC2-120", "SC2-120", "Wraase", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: an audited implementation uses empirical porch/sync timing that is not normative."},
        {"wraase-sc2-180", "Wraase SC2-180", "SC2-180", "Wraase", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; authoritative timing and an independent vector remain required."},

        {"pasokon-p3", "Pasokon P3", "P3", "Pasokon", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; sequential-colour ordering needs an independent vector."},
        {"pasokon-p5", "Pasokon P5", "P5", "Pasokon", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; sequential-colour ordering needs an independent vector."},
        {"pasokon-p7", "Pasokon P7", "P7", "Pasokon", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; sequential-colour ordering needs an independent vector."},

        {"pd-50", "PD50", "PD50", "PD", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent RX/TX evidence is still required."},
        {"pd-90", "PD90", "PD90", "PD", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent two-line luminance/chroma evidence is still required."},
        {"pd-120", "PD120", "PD120", "PD", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent two-line luminance/chroma evidence is still required."},
        {"pd-160", "PD160", "PD160", "PD", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent two-line luminance/chroma evidence is still required."},
        {"pd-180", "PD180", "PD180", "PD", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; the available self-generated WAV is not independent evidence."},
        {"pd-240", "PD240", "PD240", "PD", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent long-duration evidence is still required."},
        {"pd-290", "PD290", "PD290", "PD", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent long-duration evidence is still required."},

        {"avt-24", "AVT24", "AVT24", "AVT", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; AVT-specific frame synchronisation and an independent vector are missing."},
        {"avt-90", "AVT90", "AVT90", "AVT", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; AVT-specific frame synchronisation and an independent vector are missing."},
        {"avt-94", "AVT94", "AVT94", "AVT", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; AVT-specific frame synchronisation and an independent vector are missing."},

        {"mp-73", "MP73", "MP73", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"mp-115", "MP115", "MP115", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"mp-140", "MP140", "MP140", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"mp-175", "MP175", "MP175", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"mr-73", "MR73", "MR73", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"mr-90", "MR90", "MR90", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"mr-115", "MR115", "MR115", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"mr-140", "MR140", "MR140", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: audited QSSTV lineage assigns extended VIS 0x4A23 to both MR140 and MR175."},
        {"mr-175", "MR175", "MR175", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Blocked,
         "Blocked: QSSTV assigns 0x4A23 but the SSTV Handbook lists 0x4C23; independent waveform validation is missing."},
        {"ml-180", "ML180", "ML180", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"ml-240", "ML240", "ML240", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"ml-280", "ML280", "ML280", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},
        {"ml-320", "ML320", "ML320", "MMSSTV extended", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent extended-VIS specification/vector missing."},

        {"mp-73-narrow", "MP73-Narrow", "MP73N", "MMSSTV narrow", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent narrow/extended-VIS specification/vector missing."},
        {"mp-110-narrow", "MP110-Narrow", "MP110N", "MMSSTV narrow", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent narrow/extended-VIS specification/vector missing."},
        {"mp-140-narrow", "MP140-Narrow", "MP140N", "MMSSTV narrow", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent narrow/extended-VIS specification/vector missing."},
        {"mc-110-narrow", "MC110-Narrow", "MC110N", "MMSSTV narrow", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent narrow/extended-VIS specification/vector missing."},
        {"mc-140-narrow", "MC140-Narrow", "MC140N", "MMSSTV narrow", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent narrow/extended-VIS specification/vector missing."},
        {"mc-180-narrow", "MC180-Narrow", "MC180N", "MMSSTV narrow", ModeClassification::AnalogSstv, CatalogStatus::Catalogued,
         "Catalogued; independent narrow/extended-VIS specification/vector missing."},

        {"fax-480", "FAX480", "FAX480", "FAX", ModeClassification::RelatedFax, CatalogStatus::Blocked,
         "Blocked related mode: audited sources disagree on 512x500 versus 512x480 geometry."},
        {"hffax", "HFFAX", "HFFAX", "FAX", ModeClassification::RelatedFax, CatalogStatus::Catalogued,
         "Catalogued related-mode category; authoritative IOC/line-rate variants are not enumerated yet."},
        {"wefax", "WEFAX", "WEFAX", "FAX", ModeClassification::RelatedFax, CatalogStatus::Catalogued,
         "Catalogued related-mode category; authoritative IOC/RPM variants and legal vectors are missing."}
    };

    std::vector<SstvModeSpec> modes;
    modes.reserve(std::size(seeds));
    for (const auto& seed : seeds) {
        modes.push_back(fromSeed(seed));
    }
    return SstvModeRegistry(std::move(modes));
}

std::vector<ModeValidationIssue> SstvModeRegistry::validate(const std::vector<SstvModeSpec>& modes)
{
    std::vector<ModeValidationIssue> issues;
    std::unordered_map<std::string, std::size_t> ids;
    std::unordered_map<std::string, std::size_t> longNames;
    std::unordered_map<std::string, std::size_t> shortNames;
    struct VisOwner final {
        std::string id;
        std::string group;
    };
    std::unordered_map<std::string, VisOwner> visOwners;

    for (std::size_t index = 0; index < modes.size(); ++index) {
        const auto& mode = modes[index];
        if (mode.id.empty()) {
            addIssue(issues, ModeValidationCode::EmptyId, mode, "mode ID must not be empty");
        } else if (!ids.emplace(mode.id, index).second) {
            addIssue(issues, ModeValidationCode::DuplicateId, mode, "mode ID must be unique");
        }
        if (mode.longName.empty()) {
            addIssue(issues, ModeValidationCode::EmptyLongName, mode, "long name must not be empty");
        } else if (!longNames.emplace(mode.longName, index).second) {
            addIssue(issues, ModeValidationCode::DuplicateLongName, mode, "long name must be unique");
        }
        if (mode.shortName.empty()) {
            addIssue(issues, ModeValidationCode::EmptyShortName, mode, "short name must not be empty");
        } else if (!shortNames.emplace(mode.shortName, index).second) {
            addIssue(issues, ModeValidationCode::DuplicateShortName, mode, "short name must be unique");
        }
        if (mode.family.empty()) {
            addIssue(issues, ModeValidationCode::EmptyFamily, mode, "mode family must not be empty");
        }
        if (mode.catalogStatus == CatalogStatus::Blocked && mode.statusNote.empty()) {
            addIssue(issues, ModeValidationCode::BlockedWithoutNote, mode,
                     "blocked catalogue rows require an exact blocker note");
        }
        if ((mode.rxStatus == CapabilityStatus::Blocked
             || mode.txStatus == CapabilityStatus::Blocked
             || mode.autoDetectStatus == CapabilityStatus::Blocked)
            && mode.statusNote.empty()) {
            addIssue(issues, ModeValidationCode::BlockedWithoutNote, mode,
                     "blocked capabilities require an exact blocker note");
        }

        validateProtocolFields(mode, issues);
        validateVis(mode, issues);
        validateCapability(mode, mode.rxStatus, "RX", issues);
        validateCapability(mode, mode.txStatus, "TX", issues);
        validateCapability(mode, mode.autoDetectStatus, "auto-detect", issues);

        if (mode.catalogStatus == CatalogStatus::Verified) {
            if (!mode.protocolDataComplete || !hasVerifiedCapability(mode)) {
                addIssue(issues, ModeValidationCode::InconsistentStatus, mode,
                         "verified catalogue state requires complete data and a verified capability");
            }
            if (!mode.hasIndependentEvidence()) {
                addIssue(issues, ModeValidationCode::VerifiedWithoutIndependentEvidence, mode,
                         "verified catalogue state requires independent interoperability evidence");
            }
        }

        if (mode.vis.has_value()) {
            const auto addVisOwner = [&](const std::string& key) {
                const auto existing = visOwners.find(key);
                if (existing == visOwners.end()) {
                    visOwners.emplace(key, VisOwner {mode.id, mode.vis->documentedSharedCodeGroup});
                    return;
                }
                const bool documentedShare = !mode.vis->documentedSharedCodeGroup.empty()
                    && mode.vis->documentedSharedCodeGroup == existing->second.group;
                if (!documentedShare) {
                    addIssue(issues, ModeValidationCode::DuplicateVisCode, mode,
                             "VIS code is also owned by mode " + existing->second.id);
                }
            };

            if (mode.vis->standardCode.has_value()) {
                addVisOwner(standardVisKey(*mode.vis->standardCode));
            }
            for (const auto alias : mode.vis->standardAliases) {
                addVisOwner(standardVisKey(alias));
            }
            if (!mode.vis->extendedSequence.empty()) {
                addVisOwner(extendedVisKey(mode.vis->extendedSequence));
            }
        }
    }
    return issues;
}

const std::vector<SstvModeSpec>& SstvModeRegistry::modes() const noexcept
{
    return m_modes;
}

const SstvModeSpec* SstvModeRegistry::findById(std::string_view id) const noexcept
{
    const auto found = std::find_if(m_modes.begin(), m_modes.end(),
                                    [id](const SstvModeSpec& mode) { return mode.id == id; });
    return found == m_modes.end() ? nullptr : &*found;
}

const SstvModeSpec* SstvModeRegistry::findByName(std::string_view name) const noexcept
{
    const auto found = std::find_if(m_modes.begin(), m_modes.end(),
                                    [name](const SstvModeSpec& mode) {
                                        return mode.longName == name || mode.shortName == name;
                                    });
    return found == m_modes.end() ? nullptr : &*found;
}

std::vector<ModeValidationIssue> SstvModeRegistry::validationIssues() const
{
    return validate(m_modes);
}

bool SstvModeRegistry::isValid() const
{
    return validationIssues().empty();
}

bool containsValidationIssue(const std::vector<ModeValidationIssue>& issues,
                             ModeValidationCode code) noexcept
{
    return std::any_of(issues.begin(), issues.end(),
                       [code](const ModeValidationIssue& issue) { return issue.code == code; });
}

} // namespace decodium::sstv
