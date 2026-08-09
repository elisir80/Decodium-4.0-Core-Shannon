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
      // 1.0.538 iu8lmc - ci si presenta come "Decodium". Con id "WSJTX" i
      // collettori leggevano la nostra versione (1.0.x) come se fosse quella
      // di WSJT-X, la confrontavano con il 2.7.x e scartavano ogni pacchetto
      // con "OLD software version". I comandi in arrivo indirizzati a
      // "WSJTX"/"WSJT-X" continuano a essere accettati come alias.
      id = QStringLiteral ("Decodium");
    }
  if (id.size () > 64)
    {
      id = id.left (64).trimmed ();
    }
  return id;
}
}
}
