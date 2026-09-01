// FtxApStorico.cpp — vedi FtxApStorico.hpp per il perche' e per i numeri.
#include "Detector/FtxApStorico.hpp"

#include <algorithm>
#include <atomic>
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

namespace
{

// Un nominativo standard ha almeno una cifra e almeno tre caratteri, e non e'
// uno dei qualificatori che compaiono nel secondo campo di una chiamata
// diretta. Il controllo e' volutamente largo: chi costruisce i bit rifiuta
// comunque tutto cio' che non e' codificabile, e qui un falso positivo costa
// solo un'ipotesi inutile.
bool sembra_call (char const* t)
{
  if (!t) return false;
  int n = 0;
  bool cifra = false;
  for (; t[n] && t[n] != ' '; ++n)
    {
      if (t[n] >= '0' && t[n] <= '9') cifra = true;
    }
  return n >= 3 && n <= 13 && cifra;
}

}  // namespace

void registra_da_messaggio (int ciclo, float freq_hz, char const* messaggio)
{
  if (!messaggio) return;

  // I campi si copiano perche' il messaggio arriva da un buffer a lunghezza
  // fissa riempito di spazi, non da una stringa terminata.
  char campi[3][kLunghezzaCall] {};
  int nc = 0;
  int i = 0;
  while (messaggio[i] && nc < 3)
    {
      while (messaggio[i] == ' ') ++i;
      if (!messaggio[i]) break;
      int k = 0;
      while (messaggio[i] && messaggio[i] != ' ' && k < kLunghezzaCall - 1)
        {
          campi[nc][k++] = messaggio[i++];
        }
      campi[nc][k] = '\0';
      while (messaggio[i] && messaggio[i] != ' ') ++i;   // campo troppo lungo
      ++nc;
    }
  if (nc < 2) return;

  // Il secondo campo, o il terzo se il secondo e' un qualificatore.
  char const* mittente = sembra_call (campi[1]) ? campi[1]
                       : (nc > 2 && sembra_call (campi[2]) ? campi[2] : nullptr);
  if (!mittente) return;
  registra (ciclo, freq_hz, mittente);
}

namespace
{
std::atomic<int> g_ciclo {0};
}

int avanza_ciclo ()
{
  return g_ciclo.fetch_add (1, std::memory_order_relaxed) + 1;
}

int ciclo_corrente ()
{
  return g_ciclo.load (std::memory_order_relaxed);
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
