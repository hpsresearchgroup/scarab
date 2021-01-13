/**
 * @file mtage_unlimited.cc
 * @brief P. Michaud and A. Seznec
 * @version 0.1
 * @date 2021-01-12
 *
 * Adapted to Scarab by Stephen Pruett
 *
 * Code is derived from P.Michaud and A. Seznec code for the CBP4 winner Sorry
 * two very different code writing styles
 *
 * ////////////// STORAGE BUDGET JUSTIFICATION ////////////////
 *   unlimited size
 */

#ifndef _MTAGE_H_
#define _MTAGE_H_

#include <assert.h>
#include <cmath>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "stdint.h"
#include "stdio.h"

#include "cbp_to_scarab.h"

/*************Code From CBP 2016***************/

// initial code by P.Michaud for the CBP4 poTAGE  and poTAGE +SC

// NPRED : number of tage predictors
#define NPRED 6


// SPSIZE : spectrum size (number of subpaths) for each tage
// P0 = global, P1 = per-address, P2 = per-set, P3 = per-set, P4 = frequency
#define P0_SPSIZE 1
#define P1_SPSIZE 4096
#define P2_SPSIZE 64
#define P3_SPSIZE 16
#define P4_SPSIZE 8
#define P5_SPSIZE 1
// P2_PARAM and P3_PARAM are the log2 of the set sizes in the per-set tages
#define P2_PARAM 7
#define P3_PARAM 4

// tage parameters:

// NUMG = number of tagged tables
// LOGB = log2 of the number of entries of the tagless (bimodal) table
// LOGG = log2 of the number of entries of each tagged table
// MAXHIST = maximum path length ("rightmost" tagged table), in branches
// MINHIST = minimum path length ("leftmost" tagged table), in branches
// HASHPARAM = parameter used in the hash functions (may need to be changed with
// predictor size) RAMPUP = ramp-up period in mispredictions (should be kept
// roughly proportional to predictor size) TAGBITS = tag width in bits CTRBITS =
// width of the taken/not-taken counters in the tagless (bimodal) and tagged
// tables PATHBITS = number of per-branch address bits injected in the path
// hashing POSTPBITS = width of the taken/not-taken counters in the
// post-predictor POSTPEXTRA = number of secondary hits feeding the
// post-predictor ALLOCFAILMAX : used for clearing u bits (cf. ISL_TAGE, Andre
// Seznec, MICRO 2011) MAXALLOC = maximum number of entries stolen upon a
// misprediction (cf. ISL_TAGE) CAPHIST = path length beyond which aggressive
// update (ramp-up) is made sligtly less aggressive

// parameters specific to the global tage
#define P0_NUMG 25
#define P0_LOGB 21
#define P0_LOGG 21
#define P0_MAXHIST 5000
#define P0_MINHIST 7
#define P0_HASHPARAM 3
#define P0_RAMPUP 100000

// parameters specific to the per-address tage
#define P1_NUMG 22
#define P1_LOGB 20
#define P1_LOGG 20
#define P1_MAXHIST 2000
#define P1_MINHIST 5
#define P1_HASHPARAM 3
#define P1_RAMPUP 100000

// parameters specific to the first per-set tage
#define P2_NUMG 21
#define P2_LOGB 20
#define P2_LOGG 20
#define P2_MAXHIST 500
#define P2_MINHIST 5
#define P2_HASHPARAM 3
#define P2_RAMPUP 100000

// parameters specific to second per-set tage
#define P3_NUMG 20
#define P3_LOGB 20
#define P3_LOGG 20
#define P3_MAXHIST 500
#define P3_MINHIST 5
#define P3_HASHPARAM 3
#define P3_RAMPUP 100000

// parameters specific to the frequency-based tage
#define P4_NUMG 20
#define P4_LOGB 20
#define P4_LOGG 20
#define P4_MAXHIST 500
#define P4_MINHIST 5
#define P4_HASHPARAM 3
#define P4_RAMPUP 100000

// parameters specific to the  tage
#define P5_NUMG 20
#define P5_LOGB 20
#define P5_LOGG 20
#define P5_MAXHIST 400
#define P5_MINHIST 5
#define P5_HASHPARAM 3
#define P5_RAMPUP 100000

// parameters common to all tages
#define TAGBITS 15
#define CTRBITS 3
#define PATHBITS 6
#define POSTPBITS 5
//#define POSTPEXTRA 1
#define ALLOCFAILMAX 511
#define MAXALLOC 3
#define CAPHIST 200

// BFTSIZE = number of entries in the branch frequency table (BFT)
#define BFTSIZE (1 << 20)

// FRATIOBITS = log2 of the ratio between adjacent frequency bins (predictor P3)
#define FRATIOBITS 1

// COLT parameters (each COLT entry has 2^NPRED counters)
// LOGCOLT = log2 of the number of COLT entries
// COLTBITS = width of the taken/not-taken COLT counters
#define LOGCOLT 20
#define COLTBITS 5


using namespace std;


class path_history {
  // path history register
 public:
  int       ptr;
  int       hlength;
  unsigned* h;

  void      init(int hlen);
  void      insert(unsigned val);
  unsigned& operator[](int n);
};


class compressed_history {
  // used in the hash functions
 public:
  unsigned comp;
  int      clength;
  int      olength;
  int      nbits;
  int      outpoint;
  unsigned mask1;
  unsigned mask2;

  compressed_history();
  void reset();
  void init(int original_length, int compressed_length, int injected_bits);
  void rotateleft(unsigned& x, int m);
  void update(path_history& ph);
};


class coltentry {
  // COLT entry (holds 2^NPRED counters)
 public:
  int8_t c[1 << NPRED];
  coltentry();
  int8_t& ctr(bool predtaken[NPRED]);
};


class colt {
  // This is COLT, a method invented by Gabriel Loh and Dana Henry
  // for combining several different predictors (see PACT 2002)
 public:
  coltentry c[1 << LOGCOLT];
  int8_t&   ctr(uint64_t pc, bool predtaken[NPRED]);
  bool      predict(uint64_t pc, bool predtaken[NPRED]);
  void      update(uint64_t pc, bool predtaken[NPRED], bool taken);
};


class bftable {
  // branch frequency table (BFT)
 public:
  int freq[BFTSIZE];
  bftable();
  int& getfreq(uint64_t pc);
};


class subpath {
  // path history register and hashing
 public:
  path_history        ph;
  int                 numg;
  compressed_history* chg;
  compressed_history* chgg;
  compressed_history* cht;
  compressed_history* chtt;

  void init(int ng, int hist[], int logg, int tagbits, int pathbits, int hp);
  void init(int ng, int minhist, int maxhist, int logg, int tagbits,
            int pathbits, int hp);
  void update(uint64_t targetpc, bool taken);
  unsigned cg(int bank);
  unsigned cgg(int bank);
  unsigned ct(int bank);
  unsigned ctt(int bank);
};


class spectrum {
  // path spectrum (= set of subpaths, aka first-level history)
 public:
  int      size;
  subpath* p;

  spectrum();
  void init(int sz, int ng, int minhist, int maxhist, int logg, int tagbits,
            int pathbits, int hp);
};


class freqbins {
  // frequency bins for predictor P3
 public:
  int nbins;
  int maxfreq;

  void init(int nb);
  int  find(int bfreq);
  void update(int bfreq);
};


class gentry {
  // tage tagged tables entry
 public:
  int16_t tag;
  int8_t  ctr;
  int8_t  u;
  gentry();
};


class tage {
  // cf. TAGE (Seznec & Michaud JILP 2006, Seznec MICRO 2011)
 public:
  string name;

  int8_t*     b;  // tagless (bimodal) table
  gentry**    g;  // tagged tables
  int         bi;
  int*        gi;
  vector<int> hit;
  bool        predtaken;
  bool        altpredtaken;
  int         ppi;
  int8_t*     postp;  // post-predictor
  bool        postpredtaken;
  bool        mispred;
  int         allocfail;
  int         nmisp;

  int numg;
  int bsize;
  int gsize;
  int tagbits;
  int ctrbits;
  int postpbits;
  int postpsize;
  int rampup;
  int hashp;
  int caphist;

  tage();
  void    init(const char* nm, int ng, int logb, int logg, int tagb, int ctrb,
               int ppb, int ru, int caph);
  int     bindex(uint64_t pc);
  int     gindex(uint64_t pc, subpath& p, int bank);
  int     gtag(uint64_t pc, subpath& p, int bank);
  int     postp_index();
  gentry& getg(int i);
  bool    condbr_predict(uint64_t pc, subpath& p);
  void    uclear();
  void    galloc(int i, uint64_t pc, bool taken, subpath& p);
  void    aggressive_update(uint64_t pc, bool taken, subpath& p);
  void    careful_update(uint64_t pc, bool taken, subpath& p);
  bool    condbr_update(uint64_t pc, bool taken, subpath& p);
  void    printconfig(subpath& p);
};


/////////////////////////////////////////////////////////////

class folded_history {
  // utility class for index computation
  // this is the cyclic shift register for folding
  // a long global history into a smaller number of bits; see P. Michaud's
  // PPM-like predictor at CBP-1
 public:
  unsigned comp;
  int      CLENGTH;
  int      OLENGTH;
  int      OUTPOINT;
  void     init(int original_length, int compressed_length, int N);
  void     update(uint8_t* h, int PT);
};


class MTAGE {
 private:
  bftable  bft;
  freqbins bfreq;
  spectrum sp[NPRED];
  tage     pred[NPRED];
  subpath* subp[NPRED];
  bool     predtaken[NPRED];
  colt     co;

 public:
  MTAGE(void);
  bool GetPrediction(uint64_t PC);
  void UpdatePredictor(uint64_t PC, OpType OPTYPE, bool resolveDir,
                       bool predDir, uint64_t branchTarget);
  void TrackOtherInst(uint64_t PC, OpType opType, bool taken,
                      uint64_t branchTarget);

  void initSC();
  void HistoryUpdate(uint64_t PC, uint8_t brtype, bool taken, uint64_t target,
                     int& Y, folded_history* K, folded_history* L);


  void UpdateFinalSC(uint64_t PC, bool taken);
  void UpdateSC(uint64_t PC, bool taken, bool PRED);
  bool FinalSCpredict(uint64_t PC, bool Tpred);
  bool SCpredict(uint64_t PC, bool Tpred);
  int  percpredict(int PC, long long BHIST, int8_t* line, int PSTEP, int WIDTH);
  void updateperc(bool taken, int8_t* line, long long BHIST, int PSTEP,
                  int WIDTH);
  int  Gpredict(uint64_t PC, long long BHIST, int* length, int8_t** tab,
                int NBR);
  void Gupdate(uint64_t PC, bool taken, long long BHIST, int* length,
               int8_t** tab, int NBR, int WIDTH);

  // index function for the GEHL and MAC-RHSP tables
  // FGEHL serves to mix path history

  int gehlindex(uint64_t PC, int bank);

  int  rhspindex(uint64_t PC, int bank);
  void predict_gehl(uint64_t PC);
  void gehlupdate(uint64_t PC, bool taken);
  void predict_rhsp(uint64_t PC);
  void rhspupdate(uint64_t PC, bool taken);
  bool getloop(uint64_t PC);
  int  lindex(uint64_t);
  void loopupdate(uint64_t, bool, bool);
};
void PrintStat(double NumInst);


/***********************************************************/
#endif
