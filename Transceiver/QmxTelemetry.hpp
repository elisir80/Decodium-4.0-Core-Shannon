#ifndef QMX_TELEMETRY_HPP_
#define QMX_TELEMETRY_HPP_

#include <limits>

#include <QByteArray>

namespace decodium
{
namespace qmx_telemetry
{
  inline bool parse_unsigned_reply (QByteArray const& reply, QByteArray const& prefix,
                                    unsigned int multiplier, unsigned int * value)
  {
    if (!value || multiplier == 0)
      {
        return false;
      }

    QByteArray const frame = reply.trimmed ();
    if (!frame.startsWith (prefix) || !frame.endsWith (';'))
      {
        return false;
      }

    QByteArray const digits = frame.mid (prefix.size (), frame.size () - prefix.size () - 1);
    if (digits.isEmpty ())
      {
        return false;
      }

    unsigned int parsed = 0;
    unsigned int const maximumBeforeMultiply = std::numeric_limits<unsigned int>::max () / multiplier;
    for (char const digit : digits)
      {
        if (digit < '0' || digit > '9')
          {
            return false;
          }

        unsigned int const number = static_cast<unsigned int> (digit - '0');
        if (parsed > (maximumBeforeMultiply - number) / 10)
          {
            return false;
          }
        parsed = parsed * 10 + number;
      }

    *value = parsed * multiplier;
    return true;
  }

  // QMX PC reports tenths of a watt. Decodium stores power in milliwatts.
  inline bool parse_power_milliwatts (QByteArray const& reply, unsigned int * milliwatts)
  {
    return parse_unsigned_reply (reply, QByteArrayLiteral ("PC"), 100, milliwatts);
  }

  // QMX SW reports hundredths of an SWR unit, matching TransceiverState.
  inline bool parse_swr_hundredths (QByteArray const& reply, unsigned int * hundredths)
  {
    return parse_unsigned_reply (reply, QByteArrayLiteral ("SW"), 1, hundredths);
  }
}
}

#endif
