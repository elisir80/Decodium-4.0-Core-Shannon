#pragma once

#include <QString>

namespace decodium
{
namespace network
{
inline QString normalizedUdpClientId(QString id)
{
  id = id.simplified ();
  if (id.isEmpty ())
    {
      id = QStringLiteral ("WSJTX");
    }
  if (id.size () > 64)
    {
      id = id.left (64).trimmed ();
    }
  return id;
}
}
}
