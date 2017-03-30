#ifndef DDForwardTracking_h
#define DDForwardTracking_h 1

#include <string>

#include "marlin/Processor.h"
#include "lcio.h"
#include "EVENT/TrackerHit.h"
#include "EVENT/Track.h"
#include "IMPL/TrackImpl.h"
#include "MarlinTrk/IMarlinTrkSystem.h"

#include "KiTrack/Segment.h"
#include "KiTrack/ITrack.h"
#include "Criteria/Criteria.h"
#include "ILDImpl/SectorSystemFTD.h"
#include "ILDImpl/SectorSystemVXD.h"
#include "SectorSystemEndcap.h"
#include "EndcapHitSimple.h"

using namespace lcio ;
using namespace marlin ;
using namespace KiTrack;
using namespace KiTrackMarlin;

/** a simple typedef, making writing shorter. And it makes sense: a track consists of hits. But as a real track
 * has more information, a vector of hits can be considered as a "raw track". */
typedef std::vector< IHit* > RawTrack;


/**  Standallone Forward Tracking Processor for Marlin.<br>
 * 
 * Reconstructs the tracks through the FTD <br>
 * 
 * For a summary of what happens during each event see the method processEvent
 * 
 *  <h4>Input - Prerequisites</h4>
 *  The hits in the Forward Tracking Detector FTD
 *
 *  <h4>Output</h4> 
 *  A collection of reconstructed Tracks.
 * 
 * @param FTDHitCollections The collections containing the FTD hits <br>
 * (default value "FTDTrackerHits FTDSpacePoints" (string vector) )
 * 
 * @param ForwardTrackCollection Name of the Forward Tracking output collection<br>
 * (default value  "ForwardTracks" )
 * 
 * @param MultipleScatteringOn Whether to take multiple scattering into account when fitting the tracks<br>
 * (default value true )
 * 
 * @param EnergyLossOn Whether to take energy loss into account when fitting the tracks<br>
 * (default value true )
 * 
 * @param SmoothOn Whether to smooth all measurement sites in fit<br>
 * (default value false )
 * 
 * @param Chi2ProbCut Tracks with a chi2 probability below this will get sorted out<br>
 * (default value 0.005 )
 * 
 * @param HelixFitMax the maximum chi2/Ndf that is allowed as result of a helix fit
 * (default value 500 )
 * 
 * @param OverlappingHitsDistMax The maximum distance of hits from overlapping petals belonging to one track<br>
 * (default value 3.5 )
 * 
 * @param HitsPerTrackMin The minimum number of hits to create a track<br>
 * (default value 3 )
 * 
 * @param BestSubsetFinder The method used to find the best non overlapping subset of tracks. Available are: SubsetHopfieldNN, SubsetSimple and None.
 * None means, that no final search for the best subset is done and overlapping tracks are possible. <br>
 * (default value TrackSubsetHopfieldNN )
 * 
 * @param Criteria A vector of the criteria that are going to be used by the Cellular Automaton. <br>
 * For every criterion a min and max needs to be set!!!<br>
 * (default value is defined in class Criteria )
 * 
 * @param NameOfACriterion_min/max For every used criterion a minimum and maximum value needs to be set. <br>
 * If a criterion is named "Crit_Example", then the min parameter would be: Crit_Example_min and the max parameter Crit_Example_max.<br>
 * You can enter more than one value!!! So for example you could write something like \<parameter name="Crit_Example_min" type="float">30 0.8\</parameter>.<br>
 * This means, that if the Cellular Automaton creates too many connections (how many is defined in "MaxConnectionsAutomaton" ) it reruns
 * it with the next set of parameters. Thus allowing to tighten the cuts, if there are too many connections and so preventing 
 * it from getting stuck in very bad combinatorial situations. So in this example the Automaton will first run with the Crit_Example_min of 30
 * and if that generates too many connections, it will rerun it with the value 0.8.<br>
 * If for a criterion no further parameters are specified, the first ones will be taken on reruns.
 * 
 * @param HNN_Omega Omega for the Hopfield Neural Network; the higher omega the higher the influence of the quality indicator<br>
 * (default value 0.75)
 * 
 * @param HNN_Activation_Threshold The activation threshold for the Hopfield Neural Network<br>
 * (default value 0.5)
 * 
 * @param HNN_TInf The temperature limit of the Hopfield Neural Network<br>
 * (default value 0.1)
 * 
 * @param MaxConnectionsAutomaton If the automaton has more connections than this it will be redone with the next cut off values for the criteria.<br>
 * If there are no further new values for the criteria, the event will be skipped.<br>
 * (default value 100000 )
 * 
 * @param MaxHitsPerSector If on any single sector there are more hits than this, all the hits in the sector get dropped.
 * This is to prevent combinatorial breakdown (It is a second safety mechanism, the first one being MaxConnectionsAutomaton.
 * But if there are soooo many hits, that already the first round of the Cellular Automaton would take forever, this mechanism
 * prevents it) <br>
 * (default value 1000)
 * 
 * @author Robin Glattauer HEPHY, Wien
 *
 */
class DDForwardTracking : public Processor {
  
 public:
  
  virtual Processor*  newProcessor() { return new DDForwardTracking ; }
  
  
  DDForwardTracking() ;
  
  /** Called at the begin of the job before anything is read.
   * Use to initialize the processor, e.g. book histograms.
   */
  virtual void init() ;
  
  /** Called for every run.
   */
  virtual void processRunHeader( LCRunHeader* run ) ;
  
  /** Called for every event - the working horse.
   * 
   *  The basic procedure for reconstruction of tracks in the FTD is as follows:
   *      
   *    -# Read in all collections of hits on the FTD that are passed as steering parameters
   *    -# From every hit in these collections an FTDHit01 is created. This is, because the SegmentBuilder and the Automaton
   * need their own hit classes.
   *    -# The hits are stored in the map _map_sector_hits. The keys in this map are the sectors and the values of the map are vectors
   * of the hits within those sectors. Sector here means an integer somehow representing a place in the detector.
   * (For using this numbers and getting things like layer or side the class SectorSystemFTD is used.)
   *    -# Make a safety check to ensure no single sector is overflowing with hits. This could give a combinatorial
   * disaster leading to endless calculation times.
   *    -# Add a virtual hit in the place of the IP. It is used by the Cellular Automaton as additional information
   * (almost all reconstructable tracks come from a vertex roughly around the IP)
   *    -# Look for hits on overlapping petals. If two petals from the FTD overlap, a track may pass through both and thus
   * create two hits in close range. For pattern recognition as it is now, they are not useful. (Imagine you try to
   * guess the radius of the helix formed by a track and you have 3 hits. If these 3 hits are sensibly spaced, this is
   * no problem. But now imagine, that two of them are very close. Just a small deviation in the relative position
   * of those two would give entirely different results.) Such short connections are therefore looked for and stored, but
   * are not dealt with until later the track candidates are found.
   *    -# The SegmentBuilder takes the hits and a vector of criteria.These criteria tell the SegmentBuilder when two
   * hits might be part of a possible track. (For example, when we look for stiff tracks, we can form a line from one hit
   * to the other and demand that this line will come close to the IP. )  
   * The SegmentBuilder as the name suggests builds segments from the hits. A Segment is essentially a part of a track.
   * For now a segment consists of a single hit. BUT: in contrast to a hit it knows all other segments it is 
   * connected to. Lets take an easy example: A track crosses layer 2,3,4 and 5 creating hits A,B,C and D. 
   * Then, if the track is not very ugly (huge multiple sacttering or energy loss ) the SegmentBuilder will create 
   * corresponding segments A,B,C and D. D is connected with C. C with B, B with A and A with the IP. 
   * So in these connections the true track is already contained. 
   * For real situations we have of course many more hits and many more tracks and background and so on. So the connections
   * are plenty and if we took every possible track we could create from them, we would kill the system.
   * Therefore we use the Cellular Automaton to get rid of as much as possible.
   *    -# The Cellular Automaton: As a result from the SegmentBuilder we get an Automaton object. It has all the segments
   * the SegmentBuilder created and from us it gets some criteria to work with. (These Criteria again tell us when a
   * connection makes sense and when it doesn't. Only the connections now get longer and involve first 3 hits and then 4).
   * It first builds longer segments (now containing 2 hits instead of 1). These longer segments are again connected with
   * each other (connections are made if the criteria allow it). The Automaton then looks for connections that go all 
   * the way through to the innermost layer (the IP). Segments not connected all the way through to the IP get deleted.
   * It might be a good idea to look into <a href="../CellularAutomaton.pdf">Introduction to the Cellular Automaton</a>
   * for more details on the Cellular Automaton. The summary is, that with every step and every criterion the CA is able
   * to sort out combinatorial background until at the end we extract track candidates from it.
   *    -# Next we iterate over every trackcandidate we got.
   *    -# The hits from overlapping petals are added and every possible combination of the trackcandidate and the hits
   * we could add is created. The best version is then taken (if this is switched on in the steering parameters).
   *    -# Cuts: First we do a helix fit. If the result (chi2 / Ndf ) is too bad the track is dropped. Then we do a 
   * Kalman Fit. Also if the results here (chi squared probability) are bad the track is not saved.
   *    -# Find the best subset: the tracks we now gathered may not be all compatible with each other (i.e. share hits).
   * This situation is resolved with a best subset finder like the Hopfield Neural Network.
   *    -# Now the tracks are all compatible and suited our different criteria. It is time to save them. At the end they
   * get finalised and stored in the output collection.
   *    
   * 
   */
  virtual void processEvent( LCEvent * evt ) ; 
  
  
  virtual void check( LCEvent * evt ) ; 
  
  
  /** Called after data processing for clean up.
   */
  virtual void end() ;
  

  
 protected:
   
   /**
   * @return a map that links hits with overlapping hits on the petals behind
   * 
   * @param map_sector_hits a map with first= the sector number. second = the hits in the sector. 
   * 
   * @param secSysFTD the SectorSystemFTD that is used
   * 
   * @param distMax the maximum distance of two hits. If two hits are on the right petals and their distance is smaller
   * than this, the connection will be saved in the returned map.
   */
   /* std::map< IHit* , std::vector< IHit* > > getOverlapConnectionMap( const std::map< int , std::vector< IHit* > > & map_sector_hits,  */
   /*                                                                   const SectorSystemFTD* secSysFTD, */
   /*                                                                   float distMax); */

   std::map< IHit* , std::vector< IHit* > > getOverlapConnectionMap( const std::map< int , std::vector< IHit* > > & map_sector_hits, 
                                                                     const SectorSystemEndcap* secSysEndcap,
                                                                     float distMax);
   
   /** Adds hits from overlapping areas to a RawTrack in every possible combination.
   * 
   * @return all of the resulting RawTracks
   * 
   * @param rawTrack a RawTrack (vector of IHit* ), we want to add hits from overlapping regions
   * 
   * @param map_hitFront_hitsBack a map, where IHit* are the keys and the values are vectors of hits that
   * are in an overlapping region behind them.
   */
   std::vector < RawTrack > getRawTracksPlusOverlappingHits( RawTrack rawTrack , std::map< IHit* , std::vector< IHit* > >& map_hitFront_hitsBack );
   
   /** Finalises the track: fits it and adds TrackStates at IP, Calorimeter Face, inner- and outermost hit.
   * Sets the subdetector hit numbers and the radius of the innermost hit.
   * Also sets chi2 and Ndf.
   */
   void finaliseTrack( TrackImpl* trackImpl );
   
   /** Sets the cut off values for all the criteria
    * 
    * This method is necessary for cases where the CA just finds too much.
    * Therefore it is possible to enter a whole list of cut off values for every criterion (for every min and every max to be more precise),
    * that are then used one after the other. 
    * If the CA finds way too many connections, we can thus make the cuts tighter and rerun it. If there are still too many
    * connections, just tighten them again.
    * 
    * This method will set the according values. It will read the passed (as steering parameter) cut off values, create
    * criteria from them and store them in the corresponding vectors.
    * 
    * If there are no new cut off values for a criterion, the last one remains.
    * 
    * @return whether any new cut off value was set. false == there are no new cutoff values anymore
    * 
    * @param round The number of the round we are in. I.e. the nth time we run the Cellular Automaton.
    */
   bool setCriteria( unsigned round );
  
   // void getCellID0Info(TrackerHit*& trackerHit );
   void getCellID0Info(LCCollection*& col );

   void getCellID0AndPositionInfo(LCCollection*& col );
   /* void getCellID0AndPositionInfo(TrackerHit*& trackerHit ); */


   EndcapHitSimple* createVirtualIPHit( const SectorSystemEndcap* sectorSystemEndcap );


   /** @return Info on the content of _map_sector_hits. Says how many hits are in each sector */
   std::string getInfo_map_sector_hits();
   
   
   /** Input collection names */
   std::vector<std::string> _FTDHitCollections;
   
   /** Output collection name */
   std::string _ForwardTrackCollection;


   int _nDivisionsInPhi;
   int _nDivisionsInTheta;
   /* double _dPhi; */
   /* double _dTheta; */


   int _nRun ;
   int _nEvt ;

   /** B field in z direction */
   double _Bz;

   /** Cut for the Kalman Fit (the chi squared probability) */
   double _chi2ProbCut; 
   
   /** Cut for the Helix fit ( chi squared / degrees of freedom ) */
   double _helixFitMax; 

   // Properties of the Kalman Fit
   bool _MSOn ;
   bool _ElossOn ;
   bool _SmoothOn ;
   
   /** If this number of hits in a sector is surpassed for any sector, the hits in the sector will be dropped
    * and the quality of the output track collection will be set to poor */
   int _maxHitsPerSector;
   
   
   // Properties for the Hopfield Neural Network
   double _HNN_Omega;
   double _HNN_ActivationThreshold;
   double _HNN_TInf;
   
   /** A map to store the hits according to their sectors */
   std::map< int , std::vector< IHit* > > _map_sector_hits;
   
   /** Names of the used criteria */
   std::vector< std::string > _criteriaNames;
   
   /** Map containing the name of a criterion and a vector of the minimum cut offs for it */
   std::map< std::string , std::vector<float> > _critMinima;
   
   /** Map containing the name of a criterion and a vector of the maximum cut offs for it */
   std::map< std::string , std::vector<float> > _critMaxima;
   
   /** Minimum number of hits a track has to have in order to be stored */
   int _hitsPerTrackMin;
   
   /** A vector of criteria for 2 hits (2 1-hit segments) */
   std::vector <ICriterion*> _crit2Vec;
   
   /** A vector of criteria for 3 hits (2 2-hit segments) */
   std::vector <ICriterion*> _crit3Vec;
   
   /** A vector of criteria for 4 hits (2 3-hit segments) */
   std::vector <ICriterion*> _crit4Vec;
   
   
   // const SectorSystemFTD* _sectorSystemFTD;
   const SectorSystemEndcap* _sectorSystemEndcap;
   
   
   bool _useCED;
   
   /** the maximum distance of two hits from overlapping petals to be considered as possible part of one track */
   double _overlappingHitsDistMax;
   
   /** true = when adding hits from overlapping petals, store only the best track; <br>
    * false = store all tracksS
    */
   bool _takeBestVersionOfTrack;
   
   /** the maximum number of connections that are allowed in the automaton, if this value is surpassed, rerun
    * the automaton with tighter cuts or stop it entirely. */
   int _maxConnectionsAutomaton;
   
   /** The method used to find the best subset of tracks */
   std::string _bestSubsetFinder;
   
   unsigned _nTrackCandidates;
   unsigned _nTrackCandidatesPlus;

   
   
   MarlinTrk::IMarlinTrkSystem* _trkSystem;

   std::string _trkSystemName ;

  bool _getTrackStateAtCaloFace ;

   /** The quality of the output track collection */
   int _output_track_col_quality ; 
  
   static const int _output_track_col_quality_GOOD;
   static const int _output_track_col_quality_FAIR;
   static const int _output_track_col_quality_POOR;
  
   
} ;


/** A functor to return whether two tracks are compatible: The criterion is if they share a Hit or more */
class TrackCompatibilityShare1SP{
  
public:
   
   inline bool operator()( ITrack* trackA, ITrack* trackB ){
      
      
      std::vector< IHit* > hitsA = trackA->getHits();
      std::vector< IHit* > hitsB = trackB->getHits();
      
      
      for( unsigned i=0; i < hitsA.size(); i++){
         
         for( unsigned j=0; j < hitsB.size(); j++){
            
            if ( hitsA[i] == hitsB[j] ) return false;      // a hit is shared -> incompatible
            
         }
         
      }
      
      return true;      
      
   }
   
};


/** A functor to return the quality of a track, which is currently the chi2 probability. */
class TrackQIChi2Prob{
   
public:
   
   inline double operator()( ITrack* track ){ return track->getChi2Prob(); }
  
   
};

/** A functor to return the quality of a track.
 * 
 * For tracks with 4 hits or more the chi2prob is mapped to* 0.5-1, with p' = p/2 + 0.5.
 * Tracks with 3 hits get the chi2prob mapped to 0-0.5 by p' = p/2.
 * This way short 3-hit-tracks rank lower than 4-hit tracks.
*/
class TrackQIChi2ProbSpecial{
   
public:
   
   inline double operator()( ITrack* track ){ 
      
      if( track->getHits().size() > 3 ){
         
         return track->getChi2Prob()/2. +0.5; 
         
      }
      else{
         
         return track->getChi2Prob()/2.;
         
      }
      
   }
   
   
};




class TrackNHits{
   
public:

  inline double operator()( ITrack* track ){ return track->getHits().size(); }
   
};


#endif



