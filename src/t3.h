#ifndef T3_H
#define T3_H

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <bzlib.h>
#include <ext_socket.h>

#include <poll.h>
#include <assert.h>
#include <stdarg.h>

#include "messagereader.h"
#include "thread.h"
#include "tools.h"


class OutputFile
{
   // ====== Methods ========================================================
   public:
   OutputFile();
   ~OutputFile();
   
   bool initialize(const char* name, const bool compressFile);
   bool finish(const bool closeFile = true);
   bool printf(const char* str, ...);
   
   inline FILE* getFile() const {
      return(File);
   }
   inline size_t nextLine() {
      return(++Line);
   }

   // ====== Private Data ===================================================
   private:
   std::string Name;
   size_t      Line;
   FILE*       File;
   BZFILE*     BZFile;
};




class TrafficSpec
{
   // ====== Methods ========================================================
   public:
   TrafficSpec();
   ~TrafficSpec();

   void print(std::ostream& os) const;
   void reset();
   

   // ====== Public Data ====================================================
   public:
   static void showEntry(std::ostream& os, const double value, const uint8_t rng);

   std::string            Description;
   uint8_t                Protocol;
   std::set<unsigned int> OnOffEvents;

   double   ReliableMode;
   double   OrderedMode;

   double   OutboundFrameRate;
   double   OutboundFrameSize;
   double   InboundFrameRate;
   double   InboundFrameSize;
   uint8_t  OutboundFrameRateRng;
   uint8_t  OutboundFrameSizeRng;
   uint8_t  InboundFrameRateRng;
   uint8_t  InboundFrameSizeRng;
};



class FlowBandwidthStats
{
   // ====== Methods ========================================================
   public:
   FlowBandwidthStats();
   ~FlowBandwidthStats();

   void print(std::ostream& os,
              const double  transmissionDuration,
              const double  receptionDuration) const;
   void reset();
   

   // ====== Public Data ====================================================
   public:
   unsigned long long     TransmittedBytes;
   unsigned long long     TransmittedPackets;
   unsigned long long     TransmittedFrames;

   unsigned long long     ReceivedBytes;
   unsigned long long     ReceivedPackets;
   unsigned long long     ReceivedFrames;
   
   unsigned long long     LostBytes;
   unsigned long long     LostPackets;
   unsigned long long     LostFrames;
};




class Flow;

class FlowManager : public Thread
{
   friend class Flow;

   // ====== Methods ========================================================
   protected:
   FlowManager();
   virtual ~FlowManager();
  
   public:
   inline static FlowManager* getFlowManager() {
      return(&FlowManagerSingleton);
   }
   inline MessageReader* getMessageReader() {
      return(&Reader);
   }

   void addFlow(Flow* flow);
   void removeFlow(Flow* flow);

   void startMeasurement(const uint64_t           measurementID,
                         const bool               printFlows = false,
                         const unsigned long long now        = getMicroTime());
   void stopMeasurement(const uint64_t            measurementID,
                        const bool                printFlows = false,
                        const unsigned long long  now        = getMicroTime());
                         

   void print(std::ostream& os,
              const bool    printStatistics = false);

   Flow* findFlow(const uint64_t measurementID,
                  const uint32_t flowID,
                  const uint16_t streamID);
   Flow* findFlow(const int socketDescriptor,
                  uint16_t  streamID);
   Flow* findFlow(const struct sockaddr* from);
      
      
   // ====== Protected Methods ==============================================
   protected:
   void run();


   // ====== Private Data ===================================================
   private:
   static FlowManager FlowManagerSingleton;

   MessageReader      Reader;
   std::vector<Flow*> FlowSet;
};


class Flow : public Thread
{
   friend class FlowManager;   

   // ====== Methods ========================================================
   public:
   Flow(const uint64_t     measurementID,
        const uint32_t     flowID,
        const uint16_t     streamID,
        const TrafficSpec& trafficSpec);
   virtual ~Flow();
   

   inline uint64_t getMeasurementID() const {
      return(MeasurementID);
   }
   inline uint32_t getFlowID() const {
      return(FlowID);
   }
   inline uint16_t getStreamID() const {
      return(StreamID);
   }
   inline int getSocketDescriptor() const {
      return(SocketDescriptor);
   }
   inline const TrafficSpec& getTrafficSpec() const {
      return(Traffic);
   }

   inline const FlowBandwidthStats& getCurrentBandwidthStats() const {
      return(CurrentBandwidthStats);
   }
   inline FlowBandwidthStats& getCurrentBandwidthStats() {
      return(CurrentBandwidthStats);
   }

   inline uint32_t nextOutboundFrameID() {
      return(++LastOutboundFrameID);
   }
   inline uint64_t nextOutboundSeqNumber() {
      return(++LastOutboundSeqNumber);
   }

   inline bool isRemoteAddressValid() const {
      return(RemoteAddressIsValid);
   }
   inline const sockaddr* getRemoteAddress() const {
      return(&RemoteAddress.sa);
   }

   inline double getJitter() const {
      return(Jitter);
   }
   inline void setJitter(const double jitter) {
      Jitter = jitter;
   }
   inline double getDelay() const {
      return(Delay);
   }
   inline void setDelay(const double transitTime) {
      Delay = transitTime;
   }

   void updateTransmissionStatistics(const unsigned long long now,
                                     const size_t             addedFrames,
                                     const size_t             addedPackets,
                                     const size_t             addedBytes);
   void updateReceptionStatistics(const unsigned long long now,
                                  const size_t             addedFrames,
                                  const size_t             addedBytes,
                                  const double             delay,
                                  const double             jitter);

   
   void print(std::ostream& os, const bool printStatistics = false) const;
   void resetStatistics();

   void setSocketDescriptor(const int  socketDescriptor,
                            const bool originalSocketDescriptor = true);
   bool activate();
   bool deactivate();

   protected:
   virtual void run();
   

   // ====== Private Methods ================================================
   private:
   unsigned long long scheduleNextStatusChangeEvent(const unsigned long long now);
   unsigned long long scheduleNextTransmissionEvent() const;
      
      
   // ====== Flow Identification ============================================
   uint64_t               MeasurementID;
   uint32_t               FlowID;
   uint16_t               StreamID;

   // ====== Socket Descriptor ==============================================
   int                    SocketDescriptor;
   bool                   OriginalSocketDescriptor;
   pollfd*                PollFDEntry;   // For internal usage by FlowManager

   sctp_assoc_t           RemoteControlAssocID;
   sctp_assoc_t           RemoteDataAssocID;
   sockaddr_union         RemoteAddress;
   bool                   RemoteAddressIsValid;


   // ====== Timing =========================================================
   unsigned long long     BaseTime;
   unsigned long long     FirstTransmission;
   unsigned long long     FirstReception;
   unsigned long long     LastTransmission;
   unsigned long long   LastReception;
//    unsigned long long   NextTransmissionEvent;

   // ====== Start/stop control =============================================
   enum FlowStatus {
      WaitingForStartup = 1,
      On                = 2,
      Off               = 3
   };
   FlowStatus             Status;
//    unsigned long long     NextStatusChangeEvent;


   // ====== Traffic ????? ===============================================
   TrafficSpec          Traffic;
   uint32_t             LastOutboundFrameID;
   uint64_t             LastOutboundSeqNumber;
   
   
   // ====== Statistics =====================================================
   FlowBandwidthStats   CurrentBandwidthStats;
   FlowBandwidthStats   LastBandwidthStats;

   double               Delay;   // Transit time of latest received packet
   double               Jitter;  // Current jitter value
   OutputFile           Vector;
};

#endif
