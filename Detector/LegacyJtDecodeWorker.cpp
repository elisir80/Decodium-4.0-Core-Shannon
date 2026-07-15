// -*- Mode: C++ -*-
#include "Detector/LegacyJtDecodeWorker.hpp"

#include "Detector/JT4Decoder.hpp"
#include "Detector/JT65Decoder.hpp"
#include "Detector/JT9NarrowDecoder.hpp"
#include "Detector/JT9WideDecoder.hpp"

#include <QDebug>

namespace decodium
{
namespace legacyjt
{

LegacyJtDecodeWorker::LegacyJtDecodeWorker (QObject * parent)
  : QObject {parent}
{
}

void LegacyJtDecodeWorker::decode (DecodeRequest const& request)
{
  bool const trace = qEnvironmentVariableIsSet ("DECODIUM_JT9_TRACE");
  if (trace && request.mode == "JT9")
    {
      qInfo () << "[LEGACY-JT9] worker start"
               << "serial=" << request.serial
               << "audio=" << request.audio.size ()
               << "npts8=" << request.npts8
               << "nzhsym=" << request.nzhsym
               << "newdat=" << request.newdat
               << "ss=" << request.ss.size ()
               << "nfqso=" << request.nfqso
               << "range=" << request.nfa << "-" << request.nfb;
    }

  if (request.mode == "JT9" && request.nsubmode >= 1 && !request.ss.isEmpty ())
    {
      auto const rows = decodium::jt9wide::decode_wide_jt9 (request);
      if (trace)
        {
          qInfo () << "[LEGACY-JT9] worker done"
                   << "serial=" << request.serial << "rows=" << rows.size () << "path=wide";
        }
      Q_EMIT decodeReady (request.serial, rows);
      return;
    }

  if (request.mode == "JT65")
    {
      // Fully C++ — no Fortran runtime lock needed
      Q_EMIT decodeReady (request.serial, decodium::jt65::decode_async_jt65 (request, &m_jt65State));
      return;
    }

  if (request.mode == "JT4")
    {
      // Fully C++ — no Fortran runtime lock needed
      Q_EMIT decodeReady (request.serial,
                          decodium::jt4::decode_async_jt4 (request, &m_jt4State));
      return;
    }

  if (request.mode == "JT9")
    {
      // Fully C++ — no Fortran runtime lock needed
      auto const rows = decodium::jt9narrow::decode_async_jt9_narrow (request, &m_jt9NarrowState);
      if (trace)
        {
          qInfo () << "[LEGACY-JT9] worker done"
                   << "serial=" << request.serial << "rows=" << rows.size () << "path=narrow";
        }
      Q_EMIT decodeReady (request.serial, rows);
      return;
    }

  Q_EMIT decodeReady (request.serial, {});
}

}
}
