/*     ----------------------------------------------------------------
*
*                               sgp4unit.cpp
*
*    this file contains the sgp4 procedures for analytical propagation
*    of a satellite. the code was originally released in the 1980 and 1986
*    spacetrack papers. a detailed discussion of the theory and history
*    may be found in the 2006 aiaa paper by vallado, crawford, hujsak,
*    and kelso.
*
*                            companion code for
*               fundamentals of astrodynamics and applications
*                                    2007
*                              by david vallado
*
*       (w) 719-573-2600, email dvallado@agi.com
*
*    current :
*              30 Aug 10  david vallado
*                           delete unused variables in initl
*                           replace pow inetger 2, 3 with multiplies for speed
*    changes :
*               3 Nov 08  david vallado
*                           put returns in for error codes
*              29 sep 08  david vallado
*                           fix atime for faster operation in dspace
*                           add operationmode for afspc (a) or improved (i)
*                           performance mode
*              16 jun 08  david vallado
*                           update small eccentricity check
*              16 nov 07  david vallado
*                           misc fixes for better compliance
*              20 apr 07  david vallado
*                           misc fixes for constants
*              11 aug 06  david vallado
*                           chg lyddane choice back to strn3, constants, misc doc
*              15 dec 05  david vallado
*                           misc fixes
*              26 jul 05  david vallado
*                           fixes for paper
*                           note that each fix is preceded by a
*                           comment with "sgp4fix" and an explanation of
*                           what was changed
*              10 aug 04  david vallado
*                           2nd printing baseline working
*              14 may 01  david vallado
*                           2nd edition baseline
*                     80  norad
*                           original baseline
*       ----------------------------------------------------------------      */

#include "drv/sgp4unit.h"

static void initl
     (
       int satn,      gravconsttype whichconst,
       double ecco,   double epoch,  double inclo,   double *no,
       char *method,
       double *ainv,  double *ao,    double *con41,  double *con42, double *cosio,
       double *cosio2,double *eccsq, double *omeosq, double *posq,
       double *rp,    double *rteosq,double *sinio , double *gsto, char opsmode
     );

/*-----------------------------------------------------------------------------
*
*                           procedure initl
*
*  this procedure initializes the spg4 propagator. all the initialization is
*    consolidated here instead of having multiple loops inside other routines.
*
*  author        : david vallado                  719-573-2600   28 jun 2005
*
*  inputs        :
*    ecco        - eccentricity                           0.0 - 1.0
*    epoch       - epoch time in days from jan 0, 1950. 0 hr
*    inclo       - inclination of satellite
*    no          - mean motion of satellite
*    satn        - satellite number
*
*  outputs       :
*    ainv        - 1.0 / a
*    ao          - semi major axis
*    con41       -
*    con42       - 1.0 - 5.0 cos(i)
*    cosio       - cosine of inclination
*    cosio2      - cosio squared
*    eccsq       - eccentricity squared
*    method      - flag for deep space                    'd', 'n'
*    omeosq      - 1.0 - ecco * ecco
*    posq        - semi-parameter squared
*    rp          - radius of perigee
*    rteosq      - square root of (1.0 - ecco*ecco)
*    sinio       - sine of inclination
*    gsto        - gst at time of observation               rad
*    no          - mean motion of satellite
*
*  locals        :
*    ak          -
*    d1          -
*    del         -
*    adel        -
*    po          -
*
*  coupling      :
*    getgravconst
*    gstime      - find greenwich sidereal time from the julian date
*
*  references    :
*    hoots, roehrich, norad spacetrack report #3 1980
*    hoots, norad spacetrack report #6 1986
*    hoots, schumacher and glover 2004
*    vallado, crawford, hujsak, kelso  2006
  ----------------------------------------------------------------------------*/

static void initl
     (
       int satn,      gravconsttype whichconst,
       double ecco,   double epoch,  double inclo,   double *no,
       char *method,
       double *ainv,  double *ao,    double *con41,  double *con42, double *cosio,
       double *cosio2,double *eccsq, double *omeosq, double *posq,
       double *rp,    double *rteosq,double *sinio , double *gsto,
       char opsmode
     )
{
     /* --------------------- local variables ------------------------ */
     double ak, d1, del, adel, po, x2o3, j2, xke,
            tumin, mu, radiusearthkm, j3, j4, j3oj2;

     // sgp4fix use old way of finding gst
     double ds70;
     double ts70, tfrac, c1, thgr70, fk5r, c1p2p;
     const double twopi = 2.0 * pi;

     /* ----------------------- earth constants ---------------------- */
     // sgp4fix identify constants and allow alternate values
     xke = j2 = 0.0; /* NOTE: edit by Felix W., GCC warning @ -O3 */
     getgravconst( whichconst, &tumin, &mu, &radiusearthkm, &xke, &j2, &j3, &j4, &j3oj2 );
     x2o3   = 2.0 / 3.0;

     /* ------------- calculate auxillary epoch quantities ---------- */
     *eccsq  = ecco * ecco;
     *omeosq = 1.0 - *eccsq;
     *rteosq = sqrt(*omeosq);
     *cosio  = cos(inclo);
     *cosio2 = *cosio * *cosio;

     /* ------------------ un-kozai the mean motion ----------------- */
     ak    = pow(xke / *no, x2o3);
     d1    = 0.75 * j2 * (3.0 * *cosio2 - 1.0) / (*rteosq * *omeosq);
     del   = d1 / (ak * ak);
     adel  = ak * (1.0 - del * del - del *
             (1.0 / 3.0 + 134.0 * del * del / 81.0));
     del   = d1/(adel * adel);
     *no    = *no / (1.0 + del);

     *ao    = pow(xke / *no, x2o3);
     *sinio = sin(inclo);
     po    = *ao * *omeosq;
     *con42 = 1.0 - 5.0 * *cosio2;
     *con41 = -*con42-*cosio2-*cosio2;
     *ainv  = 1.0 / *ao;
     *posq  = po * po;
     *rp    = *ao * (1.0 - ecco);
     *method = 'n';

     // sgp4fix modern approach to finding sidereal time
     if (opsmode == 'a')
        {
         // sgp4fix use old way of finding gst
         // count integer number of days from 0 jan 1970
         ts70  = epoch - 7305.0;
         ds70 = floor(ts70 + 1.0e-8);
         tfrac = ts70 - ds70;
         // find greenwich location at epoch
         c1    = 1.72027916940703639e-2;
         thgr70= 1.7321343856509374;
         fk5r  = 5.07551419432269442e-15;
         c1p2p = c1 + twopi;
         *gsto  = fmod( thgr70 + c1*ds70 + c1p2p*tfrac + ts70*ts70*fk5r, twopi);
         if ( *gsto < 0.0 )
             *gsto = *gsto + twopi;
       }
       else
        *gsto = gstime(epoch + 2433281.5);


//#include "debug5.cpp"
}  // end initl

/*-----------------------------------------------------------------------------
*
*                             procedure sgp4init
*
*  this procedure initializes variables for sgp4.
*
*  author        : david vallado                  719-573-2600   28 jun 2005
*
*  inputs        :
*    opsmode     - mode of operation afspc or improved 'a', 'i'
*    whichconst  - which set of constants to use  72, 84
*    satn        - satellite number
*    bstar       - sgp4 type drag coefficient              kg/m2er
*    ecco        - eccentricity
*    epoch       - epoch time in days from jan 0, 1950. 0 hr
*    argpo       - argument of perigee (output if ds)
*    inclo       - inclination
*    mo          - mean anomaly (output if ds)
*    no          - mean motion
*    nodeo       - right ascension of ascending node
*
*  outputs       :
*    satrec      - common values for subsequent calls
*    return code - non-zero on error.
*                   1 - mean elements, ecc >= 1.0 or ecc < -0.001 or a < 0.95 er
*                   2 - mean motion less than 0.0
*                   3 - pert elements, ecc < 0.0  or  ecc > 1.0
*                   4 - semi-latus rectum < 0.0
*                   5 - epoch elements are sub-orbital
*                   6 - satellite has decayed
*
*  locals        :
*    cnodm  , snodm  , cosim  , sinim  , cosomm , sinomm
*    cc1sq  , cc2    , cc3
*    coef   , coef1
*    cosio4      -
*    day         -
*    dndt        -
*    em          - eccentricity
*    emsq        - eccentricity squared
*    eeta        -
*    etasq       -
*    gam         -
*    argpm       - argument of perigee
*    nodem       -
*    inclm       - inclination
*    mm          - mean anomaly
*    nm          - mean motion
*    perige      - perigee
*    pinvsq      -
*    psisq       -
*    qzms24      -
*    rtemsq      -
*    s1, s2, s3, s4, s5, s6, s7          -
*    sfour       -
*    ss1, ss2, ss3, ss4, ss5, ss6, ss7         -
*    sz1, sz2, sz3
*    sz11, sz12, sz13, sz21, sz22, sz23, sz31, sz32, sz33        -
*    tc          -
*    temp        -
*    temp1, temp2, temp3       -
*    tsi         -
*    xpidot      -
*    xhdot1      -
*    z1, z2, z3          -
*    z11, z12, z13, z21, z22, z23, z31, z32, z33         -
*
*  coupling      :
*    getgravconst-
*    initl       -
*    dscom       -
*    dpper       -
*    dsinit      -
*    sgp4        -
*
*  references    :
*    hoots, roehrich, norad spacetrack report #3 1980
*    hoots, norad spacetrack report #6 1986
*    hoots, schumacher and glover 2004
*    vallado, crawford, hujsak, kelso  2006
  ----------------------------------------------------------------------------*/

bool sgp4init
     (
       gravconsttype whichconst, char opsmode,   const int satn,     const double epoch,
       const double xbstar,  const double xecco, const double xargpo,
       const double xinclo,  const double xmo,   const double xno,
       const double xnodeo,  elsetrec *satrec
     )
{
     /* --------------------- local variables ------------------------ */
     double ao, ainv,   con42, cosio, sinio, cosio2, eccsq,
          omeosq, posq,   rp,     rteosq,
          cc1sq ,
          cc2   , cc3   , coef  , coef1 , cosio4,
	eeta  , etasq ,
          perige, pinvsq, psisq , qzms24,
           sfour ,
          temp  , temp1 , temp2 , temp3 , tsi   ,
	  xhdot1,
          qzms2t, ss, j2, j3oj2, j4, x2o3, r[3], v[3],
          tumin, mu, radiusearthkm, xke, j3, delmotemp, qzms2ttemp, qzms24temp;

     /* ------------------------ initialization --------------------- */
     // sgp4fix divisor for divide by zero check on inclination
     // the old check used 1.0 + cos(pi-1.0e-9), but then compared it to
     // 1.5 e-12, so the threshold was changed to 1.5e-12 for consistency
     const double temp4    =   1.5e-12;

     /* ----------- set all near earth variables to zero ------------ */
     satrec->isimp   = 0;   satrec->method = 'n'; satrec->aycof    = 0.0;
     satrec->con41   = 0.0; satrec->cc1    = 0.0; satrec->cc4      = 0.0;
     satrec->cc5     = 0.0; satrec->d2     = 0.0; satrec->d3       = 0.0;
     satrec->d4      = 0.0; satrec->delmo  = 0.0; satrec->eta      = 0.0;
     satrec->argpdot = 0.0; satrec->omgcof = 0.0; satrec->sinmao   = 0.0;
     satrec->t       = 0.0; satrec->t2cof  = 0.0; satrec->t3cof    = 0.0;
     satrec->t4cof   = 0.0; satrec->t5cof  = 0.0; satrec->x1mth2   = 0.0;
     satrec->x7thm1  = 0.0; satrec->mdot   = 0.0; satrec->nodedot  = 0.0;
     satrec->xlcof   = 0.0; satrec->xmcof  = 0.0; satrec->nodecf   = 0.0;

     /* ----------- set all deep space variables to zero ------------ */
     satrec->irez  = 0;   satrec->d2201 = 0.0; satrec->d2211 = 0.0;
     satrec->d3210 = 0.0; satrec->d3222 = 0.0; satrec->d4410 = 0.0;
     satrec->d4422 = 0.0; satrec->d5220 = 0.0; satrec->d5232 = 0.0;
     satrec->d5421 = 0.0; satrec->d5433 = 0.0; satrec->dedt  = 0.0;
     satrec->del1  = 0.0; satrec->del2  = 0.0; satrec->del3  = 0.0;
     satrec->didt  = 0.0; satrec->dmdt  = 0.0; satrec->dnodt = 0.0;
     satrec->domdt = 0.0; satrec->e3    = 0.0; satrec->ee2   = 0.0;
     satrec->peo   = 0.0; satrec->pgho  = 0.0; satrec->pho   = 0.0;
     satrec->pinco = 0.0; satrec->plo   = 0.0; satrec->se2   = 0.0;
     satrec->se3   = 0.0; satrec->sgh2  = 0.0; satrec->sgh3  = 0.0;
     satrec->sgh4  = 0.0; satrec->sh2   = 0.0; satrec->sh3   = 0.0;
     satrec->si2   = 0.0; satrec->si3   = 0.0; satrec->sl2   = 0.0;
     satrec->sl3   = 0.0; satrec->sl4   = 0.0; satrec->gsto  = 0.0;
     satrec->xfact = 0.0; satrec->xgh2  = 0.0; satrec->xgh3  = 0.0;
     satrec->xgh4  = 0.0; satrec->xh2   = 0.0; satrec->xh3   = 0.0;
     satrec->xi2   = 0.0; satrec->xi3   = 0.0; satrec->xl2   = 0.0;
     satrec->xl3   = 0.0; satrec->xl4   = 0.0; satrec->xlamo = 0.0;
     satrec->zmol  = 0.0; satrec->zmos  = 0.0; satrec->atime = 0.0;
     satrec->xli   = 0.0; satrec->xni   = 0.0;

     // sgp4fix - note the following variables are also passed directly via satrec->
     // it is possible to streamline the sgp4init call by deleting the "x"
     // variables, but the user would need to set the satrec->* values first. we
     // include the additional assignments in case twoline2rv is not used.
     satrec->bstar   = xbstar;
     satrec->ecco    = xecco;
     satrec->argpo   = xargpo;
     satrec->inclo   = xinclo;
     satrec->mo	    = xmo;
     satrec->no	    = xno;
     satrec->nodeo   = xnodeo;

     // sgp4fix add opsmode
     satrec->operationmode = opsmode;

     /* ------------------------ earth constants ----------------------- */
     // sgp4fix identify constants and allow alternate values
     radiusearthkm = j4 = j3oj2 = j2 = 0.0; /* NOTE: edit by Felix W., GCC warning @ -O3 */
     getgravconst( whichconst, &tumin, &mu, &radiusearthkm, &xke, &j2, &j3, &j4, &j3oj2 );
     ss     = 78.0 / radiusearthkm + 1.0;
     // sgp4fix use multiply for speed instead of pow
     qzms2ttemp = (120.0 - 78.0) / radiusearthkm;
     qzms2t = qzms2ttemp * qzms2ttemp * qzms2ttemp * qzms2ttemp;
     x2o3   =  2.0 / 3.0;

     satrec->init = 'y';
     satrec->t	 = 0.0;

     initl
         (
           satn, whichconst, satrec->ecco, epoch, satrec->inclo, &satrec->no, &satrec->method,
           &ainv, &ao, &satrec->con41, &con42, &cosio, &cosio2, &eccsq, &omeosq,
           &posq, &rp, &rteosq, &sinio, &satrec->gsto, satrec->operationmode
         );
     satrec->error = 0;

     // sgp4fix remove this check as it is unnecessary
     // the mrt check in sgp4 handles decaying satellite cases even if the starting
     // condition is below the surface of te earth
//     if (rp < 1.0)
//       {
//         printf("# *** satn%d epoch elts sub-orbital ***\n", satn);
//         satrec->error = 5;
//       }

     if ((omeosq >= 0.0 ) || ( satrec->no >= 0.0))
       {
         satrec->isimp = 0;
         if (rp < (220.0 / radiusearthkm + 1.0))
             satrec->isimp = 1;
         sfour  = ss;
         qzms24 = qzms2t;
         perige = (rp - 1.0) * radiusearthkm;

         /* - for perigees below 156 km, s and qoms2t are altered - */
         if (perige < 156.0)
           {
             sfour = perige - 78.0;
             if (perige < 98.0)
                 sfour = 20.0;
             // sgp4fix use multiply for speed instead of pow
             qzms24temp =  (120.0 - sfour) / radiusearthkm;
             qzms24 = qzms24temp * qzms24temp * qzms24temp * qzms24temp;
             sfour  = sfour / radiusearthkm + 1.0;
           }
         pinvsq = 1.0 / posq;

         tsi  = 1.0 / (ao - sfour);
         satrec->eta  = ao * satrec->ecco * tsi;
         etasq = satrec->eta * satrec->eta;
         eeta  = satrec->ecco * satrec->eta;
         psisq = fabs(1.0 - etasq);
         coef  = qzms24 * pow(tsi, 4.0);
         coef1 = coef / pow(psisq, 3.5);
         cc2   = coef1 * satrec->no * (ao * (1.0 + 1.5 * etasq + eeta *
                        (4.0 + etasq)) + 0.375 * j2 * tsi / psisq * satrec->con41 *
                        (8.0 + 3.0 * etasq * (8.0 + etasq)));
         satrec->cc1   = satrec->bstar * cc2;
         cc3   = 0.0;
         if (satrec->ecco > 1.0e-4)
             cc3 = -2.0 * coef * tsi * j3oj2 * satrec->no * sinio / satrec->ecco;
         satrec->x1mth2 = 1.0 - cosio2;
         satrec->cc4    = 2.0* satrec->no * coef1 * ao * omeosq *
                           (satrec->eta * (2.0 + 0.5 * etasq) + satrec->ecco *
                           (0.5 + 2.0 * etasq) - j2 * tsi / (ao * psisq) *
                           (-3.0 * satrec->con41 * (1.0 - 2.0 * eeta + etasq *
                           (1.5 - 0.5 * eeta)) + 0.75 * satrec->x1mth2 *
                           (2.0 * etasq - eeta * (1.0 + etasq)) * cos(2.0 * satrec->argpo)));
         satrec->cc5 = 2.0 * coef1 * ao * omeosq * (1.0 + 2.75 *
                        (etasq + eeta) + eeta * etasq);
         cosio4 = cosio2 * cosio2;
         temp1  = 1.5 * j2 * pinvsq * satrec->no;
         temp2  = 0.5 * temp1 * j2 * pinvsq;
         temp3  = -0.46875 * j4 * pinvsq * pinvsq * satrec->no;
         satrec->mdot     = satrec->no + 0.5 * temp1 * rteosq * satrec->con41 + 0.0625 *
                            temp2 * rteosq * (13.0 - 78.0 * cosio2 + 137.0 * cosio4);
         satrec->argpdot  = -0.5 * temp1 * con42 + 0.0625 * temp2 *
                             (7.0 - 114.0 * cosio2 + 395.0 * cosio4) +
                             temp3 * (3.0 - 36.0 * cosio2 + 49.0 * cosio4);
         xhdot1            = -temp1 * cosio;
         satrec->nodedot = xhdot1 + (0.5 * temp2 * (4.0 - 19.0 * cosio2) +
                              2.0 * temp3 * (3.0 - 7.0 * cosio2)) * cosio;
         //xpidot            =  satrec->argpdot+ satrec->nodedot;
         satrec->omgcof   = satrec->bstar * cc3 * cos(satrec->argpo);
         satrec->xmcof    = 0.0;
         if (satrec->ecco > 1.0e-4)
             satrec->xmcof = -x2o3 * coef * satrec->bstar / eeta;
         satrec->nodecf = 3.5 * omeosq * xhdot1 * satrec->cc1;
         satrec->t2cof   = 1.5 * satrec->cc1;
         // sgp4fix for divide by zero with xinco = 180 deg
         if (fabs(cosio+1.0) > 1.5e-12)
             satrec->xlcof = -0.25 * j3oj2 * sinio * (3.0 + 5.0 * cosio) / (1.0 + cosio);
           else
             satrec->xlcof = -0.25 * j3oj2 * sinio * (3.0 + 5.0 * cosio) / temp4;
         satrec->aycof   = -0.5 * j3oj2 * sinio;
         // sgp4fix use multiply for speed instead of pow
         delmotemp = 1.0 + satrec->eta * cos(satrec->mo);
         satrec->delmo   = delmotemp * delmotemp * delmotemp;
         satrec->sinmao  = sin(satrec->mo);
         satrec->x7thm1  = 7.0 * cosio2 - 1.0;

         /* --------------- deep space initialization ------------- */
         if ((2*pi / satrec->no) >= 225.0)
           {
             satrec->method = 'd';
             satrec->isimp  = 1;
             return false; // FIXME: disabled dpspace...
           }

       /* ----------- set variables if not deep space ----------- */
       if (satrec->isimp != 1)
         {
           cc1sq          = satrec->cc1 * satrec->cc1;
           satrec->d2    = 4.0 * ao * tsi * cc1sq;
           temp           = satrec->d2 * tsi * satrec->cc1 / 3.0;
           satrec->d3    = (17.0 * ao + sfour) * temp;
           satrec->d4    = 0.5 * temp * ao * tsi * (221.0 * ao + 31.0 * sfour) *
                            satrec->cc1;
           satrec->t3cof = satrec->d2 + 2.0 * cc1sq;
           satrec->t4cof = 0.25 * (3.0 * satrec->d3 + satrec->cc1 *
                            (12.0 * satrec->d2 + 10.0 * cc1sq));
           satrec->t5cof = 0.2 * (3.0 * satrec->d4 +
                            12.0 * satrec->cc1 * satrec->d3 +
                            6.0 * satrec->d2 * satrec->d2 +
                            15.0 * cc1sq * (2.0 * satrec->d2 + cc1sq));
         }
       } // if omeosq = 0 ...

       /* finally propogate to zero epoch to initialize all others. */
       // sgp4fix take out check to let satellites process until they are actually below earth surface
//       if(satrec->error == 0)
       sgp4(whichconst, satrec, 0.0, r, v);

       satrec->init = 'n';

//#include "debug6.cpp"
       //sgp4fix return boolean. satrec->error contains any error codes
       return true;
}  // end sgp4init

/*-----------------------------------------------------------------------------
*
*                             procedure sgp4
*
*  this procedure is the sgp4 prediction model from space command. this is an
*    updated and combined version of sgp4 and sdp4, which were originally
*    published separately in spacetrack report #3. this version follows the
*    methodology from the aiaa paper (2006) describing the history and
*    development of the code.
*
*  author        : david vallado                  719-573-2600   28 jun 2005
*
*  inputs        :
*    satrec	 - initialised structure from sgp4init() call.
*    tsince	 - time eince epoch (minutes)
*
*  outputs       :
*    r           - position vector                     km
*    v           - velocity                            km/sec
*  return code - non-zero on error.
*                   1 - mean elements, ecc >= 1.0 or ecc < -0.001 or a < 0.95 er
*                   2 - mean motion less than 0.0
*                   3 - pert elements, ecc < 0.0  or  ecc > 1.0
*                   4 - semi-latus rectum < 0.0
*                   5 - epoch elements are sub-orbital
*                   6 - satellite has decayed
*
*  locals        :
*    am          -
*    axnl, aynl        -
*    betal       -
*    cosim   , sinim   , cosomm  , sinomm  , cnod    , snod    , cos2u   ,
*    sin2u   , coseo1  , sineo1  , cosi    , sini    , cosip   , sinip   ,
*    cosisq  , cossu   , sinsu   , cosu    , sinu
*    delm        -
*    delomg      -
*    dndt        -
*    eccm        -
*    emsq        -
*    ecose       -
*    el2         -
*    eo1         -
*    eccp        -
*    esine       -
*    argpm       -
*    argpp       -
*    omgadf      -
*    pl          -
*    r           -
*    rtemsq      -
*    rdotl       -
*    rl          -
*    rvdot       -
*    rvdotl      -
*    su          -
*    t2  , t3   , t4    , tc
*    tem5, temp , temp1 , temp2  , tempa  , tempe  , templ
*    u   , ux   , uy    , uz     , vx     , vy     , vz
*    inclm       - inclination
*    mm          - mean anomaly
*    nm          - mean motion
*    nodem       - right asc of ascending node
*    xinc        -
*    xincp       -
*    xl          -
*    xlm         -
*    mp          -
*    xmdf        -
*    xmx         -
*    xmy         -
*    nodedf      -
*    xnode       -
*    nodep       -
*    np          -
*
*  coupling      :
*    getgravconst-
*    dpper
*    dpspace
*
*  references    :
*    hoots, roehrich, norad spacetrack report #3 1980
*    hoots, norad spacetrack report #6 1986
*    hoots, schumacher and glover 2004
*    vallado, crawford, hujsak, kelso  2006
  ----------------------------------------------------------------------------*/

bool sgp4
     (
       gravconsttype whichconst, elsetrec *satrec,  double tsince,
       double r[3],  double v[3]
     )
{
     double am   , axnl  , aynl , betal ,  cosim , cnod  ,
         cos2u, coseo1, cosi , cosip, cossu , cosu,
         delm , delomg, em   , emsq  ,  ecose , el2   , eo1 ,
         ep   , esine , argpm, argpp ,  argpdf, pl,     mrt = 0.0,
         mvt  , rdotl , rl   , rvdot ,  rvdotl, sinim ,
         sin2u, sineo1, sini , sinip ,  sinsu , sinu  ,
         snod , su    , t2   , t3    ,  t4    , tem5  , temp,
         temp1, temp2 , tempa, tempe ,  templ , u     , ux  ,
         uy   , uz    , vx   , vy    ,  vz    , inclm , mm  ,
         nm   , nodem, xinc , xincp ,  xl    , xlm   , mp  ,
         xmdf , xmx   , xmy  , nodedf, xnode , nodep,
         twopi, x2o3  , j2   , j3    , tumin, j4 , xke   , j3oj2, radiusearthkm,
         mu, vkmpersec, delmtemp;
     int ktr;

     /* ------------------ set mathematical constants --------------- */
     // sgp4fix divisor for divide by zero check on inclination
     // the old check used 1.0 + cos(pi-1.0e-9), but then compared it to
     // 1.5 e-12, so the threshold was changed to 1.5e-12 for consistency
     twopi = 2.0 * pi;
     x2o3  = 2.0 / 3.0;
     // sgp4fix identify constants and allow alternate values
     radiusearthkm = xke = j2 = 0.0; /* NOTE: edit by Felix W., GCC warning @ -O3 */
     getgravconst( whichconst, &tumin, &mu, &radiusearthkm, &xke, &j2, &j3, &j4, &j3oj2 );
     vkmpersec     = radiusearthkm * xke/60.0;

     /* --------------------- clear sgp4 error flag ----------------- */
     satrec->t     = tsince;
     satrec->error = 0;

     /* ------- update for secular gravity and atmospheric drag ----- */
     xmdf    = satrec->mo + satrec->mdot * satrec->t;
     argpdf  = satrec->argpo + satrec->argpdot * satrec->t;
     nodedf  = satrec->nodeo + satrec->nodedot * satrec->t;
     argpm   = argpdf;
     mm      = xmdf;
     t2      = satrec->t * satrec->t;
     nodem   = nodedf + satrec->nodecf * t2;
     tempa   = 1.0 - satrec->cc1 * satrec->t;
     tempe   = satrec->bstar * satrec->cc4 * satrec->t;
     templ   = satrec->t2cof * t2;

     if (satrec->isimp != 1)
       {
         delomg = satrec->omgcof * satrec->t;
         // sgp4fix use mutliply for speed instead of pow
         delmtemp =  1.0 + satrec->eta * cos(xmdf);
         delm   = satrec->xmcof *
                  (delmtemp * delmtemp * delmtemp -
                  satrec->delmo);
         temp   = delomg + delm;
         mm     = xmdf + temp;
         argpm  = argpdf - temp;
         t3     = t2 * satrec->t;
         t4     = t3 * satrec->t;
         tempa  = tempa - satrec->d2 * t2 - satrec->d3 * t3 -
                          satrec->d4 * t4;
         tempe  = tempe + satrec->bstar * satrec->cc5 * (sin(mm) -
                          satrec->sinmao);
         templ  = templ + satrec->t3cof * t3 + t4 * (satrec->t4cof +
                          satrec->t * satrec->t5cof);
       }

     nm    = satrec->no;
     em    = satrec->ecco;
     inclm = satrec->inclo;

     if (nm <= 0.0)
       {
//         printf("# error nm %f\n", nm);
         satrec->error = 2;
         // sgp4fix add return
         return false;
       }
     am = pow((xke / nm),x2o3) * tempa * tempa;
     nm = xke / pow(am, 1.5);
     em = em - tempe;

     // fix tolerance for error recognition
     // sgp4fix am is fixed from the previous nm check
     if ((em >= 1.0) || (em < -0.001)/* || (am < 0.95)*/ )
       {
//         printf("# error em %f\n", em);
         satrec->error = 1;
         // sgp4fix to return if there is an error in eccentricity
         return false;
       }
     // sgp4fix fix tolerance to avoid a divide by zero
     if (em < 1.0e-6)
         em  = 1.0e-6;
     mm     = mm + satrec->no * templ;
     xlm    = mm + argpm + nodem;
     emsq   = em * em;
     temp   = 1.0 - emsq;

     nodem  = fmod(nodem, twopi);
     argpm  = fmod(argpm, twopi);
     xlm    = fmod(xlm, twopi);
     mm     = fmod(xlm - argpm - nodem, twopi);

     /* ----------------- compute extra mean quantities ------------- */
     sinim = sin(inclm);
     cosim = cos(inclm);

     /* -------------------- add lunar-solar periodics -------------- */
     ep     = em;
     xincp  = inclm;
     argpp  = argpm;
     nodep  = nodem;
     mp     = mm;
     sinip  = sinim;
     cosip  = cosim;


     /* -------------------- long period periodics ------------------ */
     axnl = ep * cos(argpp);
     temp = 1.0 / (am * (1.0 - ep * ep));
     aynl = ep* sin(argpp) + temp * satrec->aycof;
     xl   = mp + argpp + nodep + temp * satrec->xlcof * axnl;

     /* --------------------- solve kepler's equation --------------- */
     u    = fmod(xl - nodep, twopi);
     eo1  = u;
     tem5 = 9999.9;
     ktr = 1;
     //   sgp4fix for kepler iteration
     //   the following iteration needs better limits on corrections
     while (( fabs(tem5) >= 1.0e-12) && (ktr <= 10) )
       {
         sineo1 = sin(eo1);
         coseo1 = cos(eo1);
         tem5   = 1.0 - coseo1 * axnl - sineo1 * aynl;
         tem5   = (u - aynl * coseo1 + axnl * sineo1 - eo1) / tem5;
         if(fabs(tem5) >= 0.95)
             tem5 = tem5 > 0.0 ? 0.95 : -0.95;
         eo1    = eo1 + tem5;
         ktr = ktr + 1;
       }

     /* ------------- short period preliminary quantities ----------- */
     ecose = axnl*coseo1 + aynl*sineo1;
     esine = axnl*sineo1 - aynl*coseo1;
     el2   = axnl*axnl + aynl*aynl;
     pl    = am*(1.0-el2);
     if (pl < 0.0)
       {
//         printf("# error pl %f\n", pl);
         satrec->error = 4;
         // sgp4fix add return
         return false;
       }
       else
       {
         rl     = am * (1.0 - ecose);
         rdotl  = sqrt(am) * esine/rl;
         rvdotl = sqrt(pl) / rl;
         betal  = sqrt(1.0 - el2);
         temp   = esine / (1.0 + betal);
         sinu   = am / rl * (sineo1 - aynl - axnl * temp);
         cosu   = am / rl * (coseo1 - axnl + aynl * temp);
         su     = atan2(sinu, cosu);
         sin2u  = (cosu + cosu) * sinu;
         cos2u  = 1.0 - 2.0 * sinu * sinu;
         temp   = 1.0 / pl;
         temp1  = 0.5 * j2 * temp;
         temp2  = temp1 * temp;

         /* -------------- update for short period periodics ------------ */
         mrt   = rl * (1.0 - 1.5 * temp2 * betal * satrec->con41) +
                 0.5 * temp1 * satrec->x1mth2 * cos2u;
         su    = su - 0.25 * temp2 * satrec->x7thm1 * sin2u;
         xnode = nodep + 1.5 * temp2 * cosip * sin2u;
         xinc  = xincp + 1.5 * temp2 * cosip * sinip * cos2u;
         mvt   = rdotl - nm * temp1 * satrec->x1mth2 * sin2u / xke;
         rvdot = rvdotl + nm * temp1 * (satrec->x1mth2 * cos2u +
                 1.5 * satrec->con41) / xke;

         /* --------------------- orientation vectors ------------------- */
         sinsu =  sin(su);
         cossu =  cos(su);
         snod  =  sin(xnode);
         cnod  =  cos(xnode);
         sini  =  sin(xinc);
         cosi  =  cos(xinc);
         xmx   = -snod * cosi;
         xmy   =  cnod * cosi;
         ux    =  xmx * sinsu + cnod * cossu;
         uy    =  xmy * sinsu + snod * cossu;
         uz    =  sini * sinsu;
         vx    =  xmx * cossu - cnod * sinsu;
         vy    =  xmy * cossu - snod * sinsu;
         vz    =  sini * cossu;

         /* --------- position and velocity (in km and km/sec) ---------- */
         r[0] = (mrt * ux)* radiusearthkm;
         r[1] = (mrt * uy)* radiusearthkm;
         r[2] = (mrt * uz)* radiusearthkm;
         v[0] = (mvt * ux + rvdot * vx) * vkmpersec;
         v[1] = (mvt * uy + rvdot * vy) * vkmpersec;
         v[2] = (mvt * uz + rvdot * vz) * vkmpersec;
       }  // if pl > 0

     // sgp4fix for decaying satellites
     if (mrt < 1.0)
       {
//         printf("# decay condition %11.6f \n",mrt);
         satrec->error = 6;
         return false;
       }

//#include "debug7.cpp"
     return true;
}  // end sgp4


/* -----------------------------------------------------------------------------
*
*                           function gstime
*
*  this function finds the greenwich sidereal time.
*
*  author        : david vallado                  719-573-2600    1 mar 2001
*
*  inputs          description                    range / units
*    jdut1       - julian date in ut1             days from 4713 bc
*
*  outputs       :
*    gstime      - greenwich sidereal time        0 to 2pi rad
*
*  locals        :
*    temp        - temporary variable for doubles   rad
*    tut1        - julian centuries from the
*                  jan 1, 2000 12 h epoch (ut1)
*
*  coupling      :
*    none
*
*  references    :
*    vallado       2004, 191, eq 3-45
* --------------------------------------------------------------------------- */

double  gstime
        (
          double jdut1
        )
   {
     const double twopi = 2.0 * pi;
     const double deg2rad = pi / 180.0;
     double       temp, tut1;

     tut1 = (jdut1 - 2451545.0) / 36525.0;
     temp = -6.2e-6* tut1 * tut1 * tut1 + 0.093104 * tut1 * tut1 +
             (876600.0*3600 + 8640184.812866) * tut1 + 67310.54841;  // sec
     temp = fmod(temp * deg2rad / 240.0, twopi); //360/86400 = 1/240, to deg, to rad

     // ------------------------ check quadrants ---------------------
     if (temp < 0.0)
         temp += twopi;

     return temp;
   }  // end gstime


/* -----------------------------------------------------------------------------
*
*                           function getgravconst
*
*  this function gets constants for the propagator. note that mu is identified to
*    facilitiate comparisons with newer models. the common useage is wgs72.
*
*  author        : david vallado                  719-573-2600   21 jul 2006
*
*  inputs        :
*    whichconst  - which set of constants to use  wgs72old, wgs72, wgs84
*
*  outputs       :
*    tumin       - minutes in one time unit
*    mu          - earth gravitational parameter
*    radiusearthkm - radius of the earth in km
*    xke         - reciprocal of tumin
*    j2, j3, j4  - un-normalized zonal harmonic values
*    j3oj2       - j3 divided by j2
*
*  locals        :
*
*  coupling      :
*    none
*
*  references    :
*    norad spacetrack report #3
*    vallado, crawford, hujsak, kelso  2006
  --------------------------------------------------------------------------- */

void getgravconst
     (
      gravconsttype whichconst,
      double *tumin,
      double *mu,
      double *radiusearthkm,
      double *xke,
      double *j2,
      double *j3,
      double *j4,
      double *j3oj2
     )
     {

       switch (whichconst)
         {
           // -- wgs-72 low precision str#3 constants --
           case wgs72old:
           *mu     = 398600.79964;        // in km3 / s2
           *radiusearthkm = 6378.135;     // km
           *xke    = 0.0743669161;
           *tumin  = 1.0 / *xke;
           *j2     =   0.001082616;
           *j3     =  -0.00000253881;
           *j4     =  -0.00000165597;
           *j3oj2  =  *j3 / *j2;
         break;
           // ------------ wgs-72 constants ------------
           case wgs72:
           *mu     = 398600.8;            // in km3 / s2
           *radiusearthkm = 6378.135;     // km
           *xke    = 60.0 / sqrt(*radiusearthkm**radiusearthkm**radiusearthkm/ *mu);
           *tumin  = 1.0 / *xke;
           *j2     =   0.001082616;
           *j3     =  -0.00000253881;
           *j4     =  -0.00000165597;
           *j3oj2  =  *j3 / *j2;
         break;
           case wgs84:
           // ------------ wgs-84 constants ------------
           *mu     = 398600.5;            // in km3 / s2
           *radiusearthkm = 6378.137;     // km
           *xke    = 60.0 / sqrt(*radiusearthkm**radiusearthkm**radiusearthkm/ *mu);
           *tumin  = 1.0 / *xke;
           *j2     =   0.00108262998905;
           *j3     =  -0.00000253215306;
           *j4     =  -0.00000161098761;
           *j3oj2  =  *j3 / *j2;
         break;
         default:
           //fprintf(stderr,"unknown gravity option (%d)\n",whichconst);
         break;
         }

     }   // end getgravconst





