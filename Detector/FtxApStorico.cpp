// FtxApStorico.cpp — vedi FtxApStorico.hpp per il perche' e per i numeri.
#include "Detector/FtxApStorico.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>
#include <vector>

namespace decodium::apstorico
{
namespace
{

struct Voce
{
  int ciclo;
  float freq;
  char call[kLunghezzaCall];
};

// L'elenco e' condiviso fra i thread di decodifica -- FT8 lavora in parallelo
// su piu' candidati -- quindi serve un lucchetto. E' preso per pochissimo: la
// lista e' corta (poche centinaia di voci) e le operazioni sono lineari su
// quella.
std::mutex g_mutex;
std::deque<Voce> g_voci;

// Un tetto duro alla memoria. In una banda molto affollata si arriva a qualche
// centinaio di decodifiche per ciclo; con dieci cicli di memoria il naturale
// sarebbe qualche migliaio. Il tetto e' molto piu' alto di cosi' e serve solo
// a impedire che una scansione andata storta faccia crescere la lista senza
// fine.
constexpr size_t kMaxVoci = 20000;

// Chi chiama e' fuori dal nostro controllo: il nominativo arriva da un campo a
// lunghezza fissa e puo' non essere terminato.
bool copia_call (char* dst, char const* src)
{
  if (!src)
    {
      return false;
    }
  int n = 0;
  while (n < kLunghezzaCall - 1 && src[n] != '\0' && src[n] != ' ')
    {
      dst[n] = src[n];
      ++n;
    }
  dst[n] = '\0';
  // Sotto i tre caratteri non e' un nominativo, e non vale la pena tenerlo.
  return n >= 3;
}

}  // namespace

void registra (int ciclo, float freq_hz, char const* nominativo)
{
  Voce v {};
  v.ciclo = ciclo;
  v.freq = freq_hz;
  if (!copia_call (v.call, nominativo))
    {
      return;
    }

  std::lock_guard<std::mutex> guardia {g_mutex};
  g_voci.push_back (v);
  while (g_voci.size () > kMaxVoci)
    {
      g_voci.pop_front ();
    }
}

int vicini (int ciclo, float freq_hz, float hz, int memoria, int max,
            char (*out)[kLunghezzaCall])
{
  if (!out || max <= 0)
    {
      return 0;
    }

  std::lock_guard<std::mutex> guardia {g_mutex};

  // Le voci troppo vecchie si buttano qui: e' l'unico punto in cui la lista
  // viene percorsa comunque, e cosi' non serve un altro passaggio di pulizia.
  int const soglia = ciclo - memoria;
  while (!g_voci.empty () && g_voci.front ().ciclo < soglia)
    {
      g_voci.pop_front ();
    }

  // Dal piu' recente al piu' vecchio, saltando i doppioni. Si scorre
  // all'indietro perche' l'ordine di inserimento e' cronologico.
  int n = 0;
  for (auto it = g_voci.rbegin (); it != g_voci.rend () && n < max; ++it)
    {
      if (it->ciclo >= ciclo)
        {
          continue;               // il ciclo corrente non e' storia
        }
      if (std::fabs (it->freq - freq_hz) > hz)
        {
          continue;
        }
      bool gia = false;
      for (int k = 0; k < n && !gia; ++k)
        {
          gia = std::strcmp (out[k], it->call) == 0;
        }
      if (gia)
        {
          continue;
        }
      std::strncpy (out[n], it->call, kLunghezzaCall - 1);
      out[n][kLunghezzaCall - 1] = '\0';
      ++n;
    }
  return n;
}

void azzera ()
{
  std::lock_guard<std::mutex> guardia {g_mutex};
  g_voci.clear ();
}

int quante ()
{
  std::lock_guard<std::mutex> guardia {g_mutex};
  return static_cast<int> (g_voci.size ());
}

}  // namespace decodium::apstorico
