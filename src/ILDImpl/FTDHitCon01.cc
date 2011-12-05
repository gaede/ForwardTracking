#include "FTDHitCon01.h"


using namespace FTrack;



FTDHitCon01::FTDHitCon01( const SectorSystemFTD* sectorSystemFTD , unsigned layerStepMax , unsigned lastLayerToIP){
   
   _sectorSystemFTD = sectorSystemFTD;
   _layerStepMax = layerStepMax;
   _lastLayerToIP = lastLayerToIP;
   
}



std::set< int > FTDHitCon01::getTargetSectors ( int sector ){
   
   
   
   std::set <int> targetSectors;
   
   
   int side = _sectorSystemFTD->getSide( sector );
   unsigned layer = _sectorSystemFTD->getLayer( sector );
//    unsigned module = _sectorSystemFTD->getModule( sector );
//    unsigned sensor = _sectorSystemFTD->getSensor( sector );
   
//    unsigned nLayers = _sectorSystemFTD->getNumberOfLayers();
   unsigned nModules = _sectorSystemFTD->getNumberOfModules();
   unsigned nSensors = _sectorSystemFTD->getNumberOfSensors();
   
   
   for( unsigned layerStep = 1; layerStep <= _layerStepMax; layerStep++ ){
      
      
      
      if ( layer >= layerStep ){ //other wise the we could jum past layer 0, this would be bad
         
         
         unsigned layerTarget = layer - layerStep;
         
         for ( unsigned iModule=0; iModule < nModules ; iModule++){ //over all modules
            
            for ( unsigned iSensor=0; iSensor < nSensors ; iSensor++ ){ //over all sensors
               
               
               targetSectors.insert( _sectorSystemFTD->getSector ( side , layerTarget , iModule , iSensor ) ); 
               
            }
            
         }
      
      }
      
   }
   
   //Allow jumping to layer 0 from layer _lastLayerToIP or less
   if ( ( layer > 1 )&& ( layer <= _lastLayerToIP ) ){
      
      
      unsigned layerTarget = 0;
      
      for ( unsigned iModule=0; iModule < nModules ; iModule++){ //over all modules
         
         for ( unsigned iSensor=0; iSensor < nSensors ; iSensor++ ){ //over all sensors
            
            
            targetSectors.insert( _sectorSystemFTD->getSector ( side , layerTarget , iModule , iSensor ) ); 
            
         }
         
      }
      
   }
   
   return targetSectors;
   
   
}

