/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2011 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author:  Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *          Ilya Moiseenko <iliamo@cs.ucla.edu>
 */

#include "ndn-forwarding-strategy.h"

#include "ns3/ndn-pit.h"
#include "ns3/ndn-pit-entry.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-content-object.h"
#include "ns3/ndn-pit.h"
#include "ns3/ndn-fib.h"
#include "ns3/ndn-content-store.h"
#include "ns3/ndn-face.h"
#include "../qtab/ndn-qtab.h"
#include "../qtab/ndn-qtab-entry.h"


#include "ns3/assert.h"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/boolean.h"
#include "ns3/string.h"

#include "ns3/ndnSIM/utils/ndn-fw-hop-count-tag.h"

#include <boost/ref.hpp>
#include <boost/foreach.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>

namespace ll = boost::lambda;

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (ForwardingStrategy);

NS_LOG_COMPONENT_DEFINE (ForwardingStrategy::GetLogName ().c_str ());

std::string
ForwardingStrategy::GetLogName ()
{
  return "ndn.fw";
}

TypeId ForwardingStrategy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::ForwardingStrategy")
    .SetGroupName ("Ndn")
    .SetParent<Object> ()

    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////

    .AddTraceSource ("OutInterests",  "OutInterests",  MakeTraceSourceAccessor (&ForwardingStrategy::m_outInterests))
    .AddTraceSource ("InInterests",   "InInterests",   MakeTraceSourceAccessor (&ForwardingStrategy::m_inInterests))
    .AddTraceSource ("DropInterests", "DropInterests", MakeTraceSourceAccessor (&ForwardingStrategy::m_dropInterests))
    .AddTraceSource ("DropInterestFailure",  "DropInterestFailure",  MakeTraceSourceAccessor (&ForwardingStrategy::m_dropInterestFailure))
    .AddTraceSource ("AggregateInterests", "AggregateInterests", MakeTraceSourceAccessor (&ForwardingStrategy::m_aggregateInterests))


    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////

    .AddTraceSource ("OutData",  "OutData",  MakeTraceSourceAccessor (&ForwardingStrategy::m_outData))
    .AddTraceSource ("InData",   "InData",   MakeTraceSourceAccessor (&ForwardingStrategy::m_inData))
    .AddTraceSource ("DataInCacheApp",   "DataInCacheApp",   MakeTraceSourceAccessor (&ForwardingStrategy::m_inDataCacheApp))
    .AddTraceSource ("DropData", "DropData", MakeTraceSourceAccessor (&ForwardingStrategy::m_dropData))

    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////

    .AddTraceSource ("SatisfiedInterests",  "SatisfiedInterests",  MakeTraceSourceAccessor (&ForwardingStrategy::m_satisfiedInterests))
    .AddTraceSource ("TimedOutInterests",   "TimedOutInterests",   MakeTraceSourceAccessor (&ForwardingStrategy::m_timedOutInterests))

    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////
    .AddTraceSource ("ExpEntry","ExpEntry",   MakeTraceSourceAccessor (&ForwardingStrategy::m_expiredEntry))


    .AddAttribute ("CacheUnsolicitedDataFromApps", "Cache unsolicited data that has been pushed from applications",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ForwardingStrategy::m_cacheUnsolicitedDataFromApps),
                   MakeBooleanChecker ())
    
    .AddAttribute ("CacheUnsolicitedData", "Cache overheard data that have not been requested",
                   BooleanValue (false),
                   MakeBooleanAccessor (&ForwardingStrategy::m_cacheUnsolicitedData),
                   MakeBooleanChecker ())

    .AddAttribute ("DetectRetransmissions", "If non-duplicate interest is received on the same face more than once, "
                                            "it is considered a retransmission",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ForwardingStrategy::m_detectRetransmissions),
                   MakeBooleanChecker ())
    ;
  return tid;
}

ForwardingStrategy::ForwardingStrategy ()
{
	myRand = CreateObject<UniformRandomVariable> ();
}

ForwardingStrategy::~ForwardingStrategy ()
{
}

void
ForwardingStrategy::NotifyNewAggregate ()
{
  if (m_pit == 0)
    {
      m_pit = GetObject<Pit> ();
    }
  if (m_fib == 0)
    {
      m_fib = GetObject<Fib> ();
    }
  if (m_contentStore == 0)
    {
      m_contentStore = GetObject<ContentStore> ();
    }
  if (m_repository == 0)
    {
      m_repository = GetObject<Repo> ();
    }
  if (m_qtab == 0)
    {
      m_qtab = GetObject<Qtab> ();
    }


  Object::NotifyNewAggregate ();
}

void
ForwardingStrategy::DoDispose ()
{
  m_pit = 0;
  m_contentStore = 0;
  m_fib = 0;
  m_repository = 0;
  m_qtab = 0;

  Object::DoDispose ();
}


void
ForwardingStrategy::SetNodeType(const std::string type)
{
	nodeType = type;
}

std::string
ForwardingStrategy::GetNodeType() const
{
	return nodeType;
}


void
ForwardingStrategy::OnInterest (Ptr<Face> inFace,
                                Ptr<const Interest> header,
                                Ptr<const Packet> origPacket)
{
  TypeId faceType = inFace->GetInstanceTypeId();
  std::string faceType_name = faceType.GetName();
  if(faceType_name.compare("ns3::ndn::AppFace") != 0)
	  m_inInterests (header, inFace, "RX", nodeType);
	  //m_inInterests (header, inFace);                  // Only Interest received from other nodes are traced as incoming Interests

	  
  //NS_LOG_UNCOND("NODE:\t" << inFace->GetNode()->GetId() << "\t Received INTEREST:\t" << header->GetName() << "\t Time:\t" << Simulator::Now().GetNanoSeconds());

  //Rimettere NS_LOG_UNCOND("FS/OnInterest - NODE:\t" << inFace->GetNode()->GetId() << "\t RX INTEREST:\t" << header->GetName() << "\ton interface:\t" << inFace->GetId() <<  "\tTime:\t" << Simulator::Now().GetMicroSeconds() << "\n");

  // *** The chunk part of the Interest must be removed for the lookup inside the QTAB
  std::stringstream ss;
  ss << header->GetName();
  std::string interest_ric = ss.str();
  ss.str("");

  Ptr<NameComponents> receivedInterest = Create<NameComponents> (interest_ric);
  uint32_t numComponents = receivedInterest->GetComponents().size();
  std::list<std::string> nome_int_ric (numComponents);
  nome_int_ric.assign(receivedInterest->begin(), receivedInterest->end());
  std::list<std::string>::iterator it = nome_int_ric.begin();
  nome_int_ric.pop_back();
  --numComponents;

  for(uint32_t t = 0; t < numComponents; t++)
  {
  ss << "/" << *it;
  it++;
  }
  std::string interestNoChunk = ss.str();
  ss.str("");

  nome_int_ric.clear();

  Ptr<Interest> header_new = Create<Interest>();
  header_new->SetName(Create<NameComponents> (interestNoChunk));

  uint32_t numInterfaces = inFace->GetNode()->GetNDevices();   // Number of Interfaces of the considered node



  // ** Check if a similar Interest has been already forwarded and not satisfied yet.
  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*header);

  // NS_LOG_UNCOND("FINE PIT LOOKUP");

  //  Check if a QTAB Entry is already present.
  Ptr<qtab::Entry> qtabEntry = m_qtab->LookupQtab(*header_new);

  // NS_LOG_UNCOND("FINE QTAB LOOKUP");

  bool similarInterest = true;

  if (pitEntry == 0)          // ** There is no matching PIT Entry.
  {
    similarInterest = false;

    pitEntry = m_pit->Create (header, header_new);

  // NS_LOG_UNCOND("FINE CREAZIONE PIT");

    if (pitEntry != 0)
    {
    	DidCreatePitEntry (inFace, header, origPacket, pitEntry);

    	//NS_LOG_UNCOND("NODE:\t" << inFace->GetNode()->GetId() << " Created PIT Entry");
    	if (qtabEntry == 0)
    	{
    		//Rimettere NS_LOG_UNCOND ("NODE:\t" << inFace->GetNode()->GetId() << "FS-OnInterest - QTAB ENTRY:\t" << header_new->GetName() << "\t NON PRESENTE!\tCreazione...\n" );
    		Ptr<fib::Entry> fibEntry = pitEntry->GetFibEntry();
    		qtabEntry = m_qtab->CreateQtab(header_new, fibEntry, numInterfaces);

		//NS_LOG_UNCOND("FINE CREAZIONE QTAB ENTRY");
    	}
    	else
    	{
    		qtabEntry->UpdateQtabEntryLifetime();
    	}
    }
    else
    {
		// NS_LOG_UNCOND ("NODE:\t" << inFace->GetNode()->GetId() << "FAILED TO CREATE PIT ENTRY\t" << header_new->GetName());

      FailedToCreatePitEntry (inFace, header, origPacket);
      return;
    }
  }

  bool isDuplicated = true;
  if (!pitEntry->IsNonceSeen (header->GetNonce ()))
  {
     pitEntry->AddSeenNonce (header->GetNonce ());      // If it is not a duplicate, the nounce is added to the list
     isDuplicated = false;
  }

  if (isDuplicated)
  {
     DidReceiveDuplicateInterest (inFace, header, origPacket, pitEntry);   // The Interest is discarded if duplicate.
     return;
  }

  // ** After having verified the Interest validity and having created the PIT entry, the node checks if the requested contents is already in the Cache.

  // ** [MT] ** In the scenario implemented here, there is a clear distinction between ordinary nodes and content providers. The former ones
  // have only the cache, so the lookup is done on the entire name of the Interest (i.e., including the last chunk component). The latter ones,
  // instead, have only the Repo, where the lookup is done considering only the main name (i.e., excluding the chunk component). So, a differentiation is needed.

  Ptr<Packet> contentObject;
  Ptr<const ContentObject> contentObjectHeader; // used for tracing
  Ptr<const Packet> payload; // used for tracing
  Ptr<ContentObject> header_repo;

  bool repo = false;

  uint32_t rp_size = m_repository->GetMaxSize();     // ** [MT] ** Ordinary nodes have a "rp_size" = 0.

  if(rp_size!=0)   // For Repo Nodes, the chunk part of the name is cut in order to do the lookup.
  {
        repo = true;
        //NS_LOG_UNCOND("REPOSITORY:\t" << inFace->GetNode()->GetId() << "\tRicevuto Interest" << "\t" << header->GetName() <<  "\t" << Simulator::Now().GetNanoSeconds());

       //NS_LOG_UNCOND("REPOSITORY:\t" << incomingFace->GetNode()->GetId() << "\tInterest senza chunk:\t" << interestNoChunk <<  "\t" << Simulator::Now().GetNanoSeconds());

        //  **** Lookup nel repository
        boost::tie (contentObject, contentObjectHeader, payload) = m_repository->Lookup (header_new);

        if(contentObject != 0)   // The REPO has the content
        {
              // NS_LOG_UNCOND("NODE:\t" << inFace->GetNode()->GetId() << " CONTENT IN REPO: " << interestNoChunk);

              header_repo = Create<ContentObject> ();
              contentObject->RemoveHeader (*header_repo);
              header_repo->SetName(Create<NameComponents>(interest_ric));
              contentObject->AddHeader(*header_repo);
        }
        else
            // NS_LOG_UNCOND("NODE:\t" << inFace->GetNode()->GetId() << " CONTENT NOT IN REPO: " << interestNoChunk);
  }
  else    // It is NOT a REPO, so the lookup is done on the entire name.
  {
      //Rimettere NS_LOG_UNCOND("NODE:\t" << inFace->GetNode()->GetId() << " CHECK CONTENT IN CACHE: " << interestNoChunk);

        boost::tie (contentObject, contentObjectHeader, payload) = m_contentStore->Lookup (header);
  }


/*  if(rp_size!=0)
  {
      //NS_LOG_UNCOND("REPO:\t" << inFace->GetNode()->GetId() << "\tInterest without chunk part:\t" << interestNoChunk <<  "\t" << Simulator::Now().GetNanoSeconds());

      //  ** Lookup inside the Repo
      boost::tie (contentObject, contentObjectHeader, payload) = m_repository->Lookup (header);
  }
  else
  {
      boost::tie (contentObject, contentObjectHeader, payload) = m_contentStore->Lookup (header);
  }
*/


  if (contentObject != 0)          //  The requested content is in the local cache (Repo or not)
  {
      NS_ASSERT (contentObjectHeader != 0);

      //NS_LOG_UNCOND ("NODONODO:\t" << incomingFace->GetNode()->GetId() << "\t CO TROVATO IN CACHE O REPO:\t" << contentObjectHeader->GetName());

      // Old method for hot count tracing. The value is initialized to '0'
      Ptr<ContentObject> header_cont = Create<ContentObject> ();
      contentObject->RemoveHeader (*header_cont);

      // *** Setting both the HopCount and the Qvalue (if the content is in cache, is the Qvalue equal to zero?)
      header_cont->GetAdditionalInfo().SetHopCount(0);
      header_cont->GetAdditionalInfo().SetQvalue(MicroSeconds(0));

      contentObject->AddHeader(*header_cont);

      pitEntry->AddIncoming (inFace);

      // Do data plane performance measurements
      //WillSatisfyPendingInterest (0, pitEntry);                 // l'Incoming face  '0' perch l'interest verrˆ soddisfatto dalla cache locale.


      // *** Check the state of the INFORM ALGORITHM
      if(qtabEntry->GetExplorationFlag())
      {
    	  if(qtabEntry->ExtractTempMinQvalue() != 0)
    		  qtabEntry->IncrementExplorationChunks();
    	  else
    	  {
    		  if (qtabEntry->GetExplorationChunks() == 0)
    			  qtabEntry->IncrementExplorationChunks();
    		  else
    			  NS_LOG_UNCOND("qTabEntry present, but no received DATA to update the estimates!!");
    	  }

        //Rimettere NS_LOG_UNCOND ("FS-OnInterest - Requested content present!\n"
        	//	<< "Algorithm for the entry:\t" << header_new->GetName()
        	//	<< "\tis in Exploration phase, with a number of sent chunks equal to:\t" << qtabEntry->GetExplorationChunks() << "\n"
        	//	<< "Piggyback of the local QVALUE equal to:" << header_cont->GetAdditionalInfo().GetQvalue() << "\n");

      	if(qtabEntry->GetExplorationChunks() > inFace->GetNode()->GetObject<Qtab>()->GetMaxChunksExploration())
      	{
      		qtabEntry->SetExplorationFlag(false);    // Switch to Exploitation phase.
      		qtabEntry->SetIsFirstExplorationFlag(false);

      		qtabEntry->SetExploitationChunks(0);
      		qtabEntry->SetExplorationChunks(0);

      		/// *** INFORM **** When switching to the exploitation phase, the minQvalue needs to be calculated (struct qMin)
      		qtabEntry->CalculateMinQvalue();

            //Rimettere NS_LOG_UNCOND ("FS-OnInterest - ** ALERT ** Max number of chunks for the exploration! Switching to EXPLOITATION!\n"
            		// << "** The evaluated MinQValue is equal to:\n"
            		// << "** ** Qvalue:\t" << qtabEntry->minQValue->qValueMin << "\n"
            		// << "** ** Interface:\t" << qtabEntry->minQValue->interfId << "\n");
      	}
      }
      else
      {
      	qtabEntry->IncrementExploitationChunks();

        //Rimettere NS_LOG_UNCOND ("FS-OnInterest - Requested content present!\n"
        	//	<< "Algorithm for the entry:\t" << header_new->GetName()
        	//	<< "\tis in fase Exploitation, with a number of chunks equal to:\t" << qtabEntry->GetExploitationChunks() << "\n"
        	//	<< "Piggyback of the local QVALUE equal to:" << header_cont->GetAdditionalInfo().GetQvalue() << "\n");


      	if(qtabEntry->GetExploitationChunks() > inFace->GetNode()->GetObject<Qtab>()->GetMaxChunksExploitation())
      	{
      		qtabEntry->SetExplorationFlag(true);    // Ritorno alla fase Exploration.
      		qtabEntry->SetIsFirstExplorationFlag(false);

      		qtabEntry->SetExplorationChunks(0);
      		qtabEntry->SetExploitationChunks(0);

            //Rimettere NS_LOG_UNCOND ("FS-OnInterest - ** ALERT ** Max number of chunk for exploitation! Switch back to EXPLORATION!\n"
            	//	<< "** The MinQValue is equal to:\n"
            	//	<< "** ** Qvalue:\t" << qtabEntry->minQValue->qValueMin << "\n"
            	//	<< "** ** Interface:\t" << qtabEntry->minQValue->interfId << "\n");
      	}

      }


      // Actually satisfy pending interest
      if(!repo) {
 		  SatisfyPendingInterest (0, contentObjectHeader, payload, contentObject, pitEntry);   // Soddisfo l'Interest ricevuto ed esco dalla funzione.
		  }
      else {
    	  //NS_LOG_UNCOND("Contenuto creato dal REPOSITORY:" << "\t" << contentObjectHeader->GetName());
          SatisfyPendingInterest (0, header_repo, payload, contentObject, pitEntry);   // Soddisfo l'Interest ricevuto ed esco dalla funzione.
	      }
      return;
  }


/*  if (contentObject != 0)            // The requested content is either in the ContentStore or in the Repo
  {
      NS_ASSERT (contentObjectHeader != 0);

      //NS_LOG_UNCOND("NODE:\t" << inFace->GetNode()->GetId() << "\tContent Object Size:\t" << contentObject->GetSize() << "\tPackect Size:\t" << payload->GetSize() << "\tContent Object Header:\t" << contentObjectHeader->GetSerializedSize());


      //FwHopCountTag hopCountTag;   // New method for the hop count tracing
      //if (origPacket->PeekPacketTag (hopCountTag))
      //{
      //    contentObject->AddPacketTag (hopCountTag);
      //}

      // Old method for the hop count tracing
      Ptr<ContentObject> header_cont = Create<ContentObject> ();
      contentObject->RemoveHeader (*header_cont);
      header_cont->GetAdditionalInfo().SetHopCount(0);
      contentObject->AddHeader(*header_cont);

      pitEntry->AddIncoming (inFace);

      // Do data plane performance measurements
      //WillSatisfyPendingInterest (0, pitEntry);

      //NS_LOG_UNCOND ("FS/OnInterest - Contenuto richiesto presente!\n");



      // Actually satisfy pending interest
      SatisfyPendingInterest (0, contentObjectHeader, payload, contentObject, pitEntry);

      return;
  }
*/
  if (similarInterest && ShouldSuppressIncomingInterest (inFace, header, origPacket, pitEntry))
  {
    pitEntry->AddIncoming (inFace/*, header->GetInterestLifetime ()*/);
    // update PIT entry lifetime
    pitEntry->UpdateLifetime (header->GetInterestLifetime ());


    // *** INFORM *** Even if the Interest is aggregated inside a PIT entry,
    //				  the number of chunks of the respective Inform phase should be increased.
    if(qtabEntry->GetExplorationFlag())
    {
    	  if(qtabEntry->ExtractTempMinQvalue() != 0)
    		  qtabEntry->IncrementExplorationChunks();
    	  else
    	  {
    		  if (qtabEntry->GetExplorationChunks() == 0)
    			  qtabEntry->IncrementExplorationChunks();
    		  else
    			  NS_LOG_UNCOND("qTabEntry present, but no received DATA to update the estimates!!");
    	  }

         //Rimettere  NS_LOG_UNCOND ("FS-OnInterest - Interest Aggregated for:\t" << header->GetName() << "\n"
          	//	<< "Algorithm for the entry:\t" << header_new->GetName()
          	//	<< "\tis in Exploration phase, with a number of sent chunks equal to:\t" << qtabEntry->GetExplorationChunks() << "\n");


    	if(qtabEntry->GetExplorationChunks() > inFace->GetNode()->GetObject<Qtab>()->GetMaxChunksExploration())
    	{
    		qtabEntry->SetExplorationFlag(false);    // Switch to Exploitation.
    		qtabEntry->SetIsFirstExplorationFlag(false);

    		qtabEntry->SetExploitationChunks(0);
    		qtabEntry->SetExplorationChunks(0);

      		/// *** INFORM **** When switching to the exploitation phase, the minQvalue needs to be calculated (struct qMin)
    		qtabEntry->CalculateMinQvalue();

            //Rimettere NS_LOG_UNCOND ("FS-OnInterest - ** ALERT ** Max num of chunks for Exploration! Switching to EXPLOITATION!\n"
            	//	<< "** The evaluated MinQValue is equal to:\n"
            	//	<< "** ** Qvalue:\t" << qtabEntry->minQValue->qValueMin << "\n"
            	//	<< "** ** Interface:\t" << qtabEntry->minQValue->interfId << "\n");
    	}
    }
    else
    {
    	qtabEntry->IncrementExploitationChunks();

        //Rimettere NS_LOG_UNCOND ("FS/OnInterest - Interest aggregated for:\t" << header->GetName() << "\n"
        	//	<< "Algorithm for the entry:\t" << header_new->GetName()
        	//	<< "\tis in Exploitation Phase, with a number of chunks equal to:\t" << qtabEntry->GetExplorationChunks() << "\n");

    	if(qtabEntry->GetExploitationChunks() > inFace->GetNode()->GetObject<Qtab>()->GetMaxChunksExploitation())
    	{
    		qtabEntry->SetExplorationFlag(true);    // Swith back to Exploration.
    		qtabEntry->SetIsFirstExplorationFlag(false);

    		qtabEntry->SetExplorationChunks(0);
    		qtabEntry->SetExploitationChunks(0);

            //Rimettere NS_LOG_UNCOND ("FS-OnInterest - ** ALERT ** Max number of chunks for Exploitation! Switch back to EXPLORATION!\n"
            		// << "** The MinQValue id equal to:\n"
            		// << "** ** Qvalue:\t" << qtabEntry->minQValue->qValueMin << "\n"
            		// << "** ** Interface:\t" << qtabEntry->minQValue->interfId << "\n");
    	}

    }


    // Suppress this interest if we're still expecting data from some other face
    NS_LOG_DEBUG ("Suppress interests");
    //m_dropInterests (header, inFace);

    //m_aggregateInterests (header, inFace);
    m_aggregateInterests (header, inFace, "AGGR", nodeType);

    //NS_LOG_UNCOND ("FS/OnInterest - Interest aggregato per:\t" << header->GetName() << "\n");

    DidSuppressSimilarInterest (inFace, header, origPacket, pitEntry);
    return;
  }

  if (similarInterest)
  {
    DidForwardSimilarInterest (inFace, header, origPacket, pitEntry);
  }

  // *** INFORM *** Either with a retransmission or with a new Interest, the number of chunks of the respective Inform phase should be increased.
  if(qtabEntry->GetExplorationFlag())
  {
    	  if(qtabEntry->ExtractTempMinQvalue() != 0)
    		  qtabEntry->IncrementExplorationChunks();
    	  else
    	  {
    		  if (qtabEntry->GetExplorationChunks() == 0)
    			  qtabEntry->IncrementExplorationChunks();
    		  else
    			  NS_LOG_UNCOND("qtabEntry present, but no received DATA to update the estimates!!");
    	  }

        //Rimettere NS_LOG_UNCOND ("FS-OnInterest - Interest forwarded for:\t" << header->GetName() << "\n"
    		// << "Algorithm for the entry:\t" << header_new->GetName()
    		// << "\tis in Exploration phase, with a number of chunks equal to:\t" << qtabEntry->GetExplorationChunks() << "\n");

  	if(qtabEntry->GetExplorationChunks() > inFace->GetNode()->GetObject<Qtab>()->GetMaxChunksExploration())
  	{
  		qtabEntry->SetExplorationFlag(false);    // Switch to Exploitation.
  		qtabEntry->SetIsFirstExplorationFlag(false);

  		qtabEntry->SetExploitationChunks(0);
  		qtabEntry->SetExplorationChunks(0);

  		/// *** INFORM **** When switching to the exploitation phase, the minQvalue needs to be calculated (struct qMin)
  		qtabEntry->CalculateMinQvalue();

        //Rimettere NS_LOG_UNCOND ("FS-OnInterest - ** ALERT ** Max number of chunks for Exploration! Switching to EXPLOITATION!\n"
        	//	<< "** The evaluated MinQValue is equal to:\n"
        	//	<< "** ** Qvalue:\t" << qtabEntry->minQValue->qValueMin << "\n"
        	//	<< "** ** Interface:\t" << qtabEntry->minQValue->interfId << "\n");
  	}
  }
  else
  {
  	qtabEntry->IncrementExploitationChunks();

    NS_LOG_UNCOND ("FS-OnInterest - Interest forwarded for:\t" << header->GetName() << "\n"
    		<< "Algorithm for the entry:\t" << header_new->GetName()
    		<< "\tis in Exploitation phase, with a number of chunks equal to:\t" << qtabEntry->GetExplorationChunks() << "\n");

  	if(qtabEntry->GetExploitationChunks() > inFace->GetNode()->GetObject<Qtab>()->GetMaxChunksExploitation())
  	{
  		qtabEntry->SetExplorationFlag(true);    // Switch back to Exploration.
  		qtabEntry->SetIsFirstExplorationFlag(false);

  		qtabEntry->SetExplorationChunks(0);
  		qtabEntry->SetExploitationChunks(0);

        //Rimettere NS_LOG_UNCOND ("FS-OnInterest - ** ALERT ** Max number of chunks for Exploitation! Switch back to EXPLORATION!\n"
        	//	<< "** The MinQValue is equal to:\n"
        	//	<< "** ** Qvalue:\t" << qtabEntry->minQValue->qValueMin << "\n"
        	//	<< "** ** Interface:\t" << qtabEntry->minQValue->interfId << "\n");
  	}

  }


  // At this point, the Interest must be forwarded according to the choosen forwarding strategy.
  // At this point, the Interest must be forwarded according to the choosen forwarding strategy.
  PropagateInterestQtab (inFace, header, origPacket, pitEntry, qtabEntry);
}

void
ForwardingStrategy::OnData (Ptr<Face> inFace,
                            Ptr<const ContentObject> header,
                            Ptr<Packet> payload,
                            Ptr<const Packet> origPacket)
{
  //NS_LOG_FUNCTION (incomingFace << header->GetName () << payload << packet);
  //m_inData (header, inFace);     // ** [MT] ** Only the requested data will be cached

  //NS_LOG_UNCOND ("NODE:\t" << inFace->GetNode()->GetId() << "\t L3 RECEIVED DATA:\t" << header->GetName() << "\t" << Simulator::Now().GetNanoSeconds());

  // Lookup PIT entry
  uint32_t incInterfId = inFace->GetId();
  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*header);
  if (pitEntry == 0)
  {
      //DidReceiveUnsolicitedData (inFace, header, payload);
      //m_dropData (header, payload, inFace);
      return;
  }
  else
  {
	// m_inData (header, inFace);
     //   m_inData (header, 0, inFace, "Rx");    NB: Tracing Disabled because we trace the Frw Data (i.e., 1 frw = 1 rcv)

	 m_contentStore->Add (header, payload);     // Add or update entry in the content store

	 //NS_LOG_UNCOND ("NODE:\t" << inFace->GetNode()->GetId() << "\t PIT Entry found:\t" << "\" " << header->GetName() << "\" :\t" << pitEntry->GetPrefix());
	 //NS_LOG_UNCOND ("FS/OnData - NODE:\t" << inFace->GetNode()->GetId() << "\t RX DATA:\t" << header->GetName() << "\t from interface:\t" << inFace->GetId() << "\tTime:\t" << Simulator::Now().GetMicroSeconds() << "\n");

	 //////////////////////////////////////////////////////////
	 /* *** INFORM ***
	  * 			When a DATA is received, we must:
	  * 			1) Extract the piggybacked qValue and calculate the qValue associated the the considered entry and
	  * 			to the incoming face of the Interest;
	  * 			2) Calculate the best qValue assoiated to the entry and insert that inside the forwarded Data.
	  */

	  // *******  Cut the chunk part of the retrieved Data  *****

	  std::stringstream ss;
	  ss << header->GetName();
	  std::string dataRic = ss.str();
	  ss.str("");

	  Ptr<Name> receivedData = Create<Name> (dataRic);
	  uint32_t numComponents = receivedData->GetComponents().size();

	  std::list<std::string> nameDataRec (numComponents);
	  nameDataRec.assign(receivedData->begin(), receivedData->end());
	  std::list<std::string>::iterator it = nameDataRec.begin();
	  nameDataRec.pop_back();
	  --numComponents;

	  for(uint32_t t = 0; t < numComponents; t++)    // Reconstruct the name without the chunk part
	  {
		    ss << "/" << *it;
		    it++;
	  }
	  std::string dataNoChunk = ss.str();
	  ss.str("");

	  nameDataRec.clear();


	  Ptr<Packet> packetCopy = Create<Packet> ();
	  Ptr<ContentObject> headerNew = Create<ContentObject>();
	  packetCopy = origPacket->Copy();
	  packetCopy->RemoveHeader(*headerNew);

	  headerNew->SetName(Create<Name> (dataNoChunk));
	  Ptr<qtab::Entry> qtabEntry = m_qtab->LookupQtab(*headerNew);

	  if(qtabEntry == 0)
	  {
		  NS_LOG_UNCOND("FS-OnData - ** ERROR ** QtabEntry:\t" << headerNew->GetName() << "\tnot present when receiving the DATA!");
	  }

	  // Exctract the piggybacked qValue and update the correspondent one inside the local Tab.
	  Time piggyQvalue = headerNew->GetAdditionalInfo().GetQvalue();
	  //Rimettere NS_LOG_UNCOND("FS-OnData - Piggybacked Qvalue:\t" << piggyQvalue.GetMicroSeconds() << " us\n");
	  //Rimettere NS_LOG_UNCOND("FS-OnData - Qvalue befor the update:\t" << qtabEntry->GetqValuesInterf().operator [](inFace->GetId()).GetMicroSeconds() << " us\n");

	  qtabEntry->UpdateQvalue(inFace->GetId(), piggyQvalue);

	  //Rimettere NS_LOG_UNCOND("FS/OnData - Qvalue after the update:\t" << qtabEntry->GetqValuesInterf().operator [](inFace->GetId()).GetMicroSeconds() << " us\n");

	  Time tempMinQvalue;

	  bool explorePhase = qtabEntry->GetExplorationFlag();

	  if(!explorePhase) // Sono nella Exploitation Phase
	  {
		  // Check con il valore assoluto per vedere se permanere nella exploitation phase
		  bool continueExploit = qtabEntry->CheckContinueExploit(incInterfId);

		  if(continueExploit)
		  {
			  //Rimettere NS_LOG_UNCOND("FS-OnData - EXPLOITATION PHASE - The variation of the MinQvalue is UNDER the DELTA...Go ahead with Exploitation...\n");
			  tempMinQvalue = qtabEntry->minQValue->qValueMin;
			  headerNew->GetAdditionalInfo().SetQvalue(tempMinQvalue);
		  }
		  else
		  {
			  //Rimettere NS_LOG_UNCOND("FS-OnData - EXPLOITATION PHASE - The variation of the MinQvalue is OVER the DELTA...Switch back to Exploration...\n");
			  qtabEntry->SetExplorationFlag(true);
			  qtabEntry->SetIsFirstExplorationFlag(false);

			  qtabEntry->SetExplorationChunks(0);
			  qtabEntry->SetExploitationChunks(0);

			  tempMinQvalue = qtabEntry->minQValue->qValueMin;
			  headerNew->GetAdditionalInfo().SetQvalue(tempMinQvalue);
		  }
	  }
	  else
	  {
		  tempMinQvalue = qtabEntry->ExtractTempMinQvalue();
		  //Rimettere NS_LOG_UNCOND("FS-OnData - EXPLORATION PHASE - Calculate the qMinValue that should be piggybacked...\tThe result is:\t" << tempMinQvalue.GetMicroSeconds() << " us");
		  headerNew->GetAdditionalInfo().SetQvalue(tempMinQvalue);
	  }

	  // Reinsert the full name
	  headerNew->SetName(Create<Name> (dataRic));

	  packetCopy->AddHeader(*headerNew);

	  //////////////////////////////////////////////////////////////////////



	 while (pitEntry != 0)
	 {
	     // Do data plane performance measurements
	     //WillSatisfyPendingInterest (inFace, pitEntry);

	     // Actually satisfy pending interest
	     SatisfyPendingInterest (inFace, header, payload, origPacket, pitEntry);

	     // Lookup another PIT entry
	     pitEntry = m_pit->Lookup (*header);    // ** [MT] ** In this implementation, the lookup in the PIT when receiving a Data is an exact match;
	     	 	 	 	 	 	 	 	 	 	//            so, only the Interest(s) with the same name of the received Data will be satisfied.
	 }
  }
}


void
ForwardingStrategy::DidCreatePitEntry (Ptr<Face> inFace,
                                       Ptr<const Interest> header,
                                       Ptr<const Packet> origPacket,
                                       Ptr<pit::Entry> pitEntrypitEntry)
{
}

void
ForwardingStrategy::FailedToCreatePitEntry (Ptr<Face> inFace,
                                            Ptr<const Interest> header,
                                            Ptr<const Packet> origPacket)
{
  m_dropInterests (header, inFace);
}

bool
ForwardingStrategy::WillSendOutInterest (Ptr<Face> outFace,
                                             Ptr<const Interest> header,
                                             Ptr<pit::Entry> pitEntry)
{
  pit::Entry::out_iterator outgoing =
    pitEntry->GetOutgoing ().find (outFace);

  if (outgoing != pitEntry->GetOutgoing ().end () && outgoing->m_retxCount >= pitEntry->GetMaxRetxCount ())
    {
      NS_LOG_ERROR (outgoing->m_retxCount << " >= " << pitEntry->GetMaxRetxCount ());
      //NS_LOG_UNCOND("INTERFACCIA GIA' UTILIZZATA e RTX COUNT > MAX RTX! NODO:\t" << outFace->GetNode()->GetId()); 
      return false; // already forwarded before during this retransmission cycle
    }


  bool ok = outFace->IsUp ();
  if (!ok)
    return false;

  pitEntry->AddOutgoing (outFace);
  return true;
}


void
ForwardingStrategy::DidReceiveDuplicateInterest (Ptr<Face> inFace,
                                                 Ptr<const Interest> header,
                                                 Ptr<const Packet> origPacket,
                                                 Ptr<pit::Entry> pitEntry)
{
  /////////////////////////////////////////////////////////////////////////////////////////
  //                                                                                     //
  // !!!! IMPORTANT CHANGE !!!! Duplicate interests will create incoming face entry !!!! //
  //                                                                                     //
  /////////////////////////////////////////////////////////////////////////////////////////
  //pitEntry->AddIncoming (inFace);
  //m_dropInterests (header, inFace);
}

void
ForwardingStrategy::DidSuppressSimilarInterest (Ptr<Face> face,
                                                Ptr<const Interest> header,
                                                Ptr<const Packet> origPacket,
                                                Ptr<pit::Entry> pitEntry)
{
}

void
ForwardingStrategy::DidForwardSimilarInterest (Ptr<Face> inFace,
                                               Ptr<const Interest> header,
                                               Ptr<const Packet> origPacket,
                                               Ptr<pit::Entry> pitEntry)
{
}

void
ForwardingStrategy::DidExhaustForwardingOptions (Ptr<Face> inFace,
                                                 Ptr<const Interest> header,
                                                 Ptr<const Packet> origPacket,
                                                 Ptr<pit::Entry> pitEntry)
{
  NS_LOG_FUNCTION (this << boost::cref (*inFace));

  //NS_LOG_UNCOND("EXHAUST FORWARDING OPTIONS! NODO:\t" << inFace->GetNode()->GetId());
  
  if (pitEntry->AreAllOutgoingInVain ())
    {
      m_dropInterests (header, inFace);

      // All incoming interests cannot be satisfied. Remove them
      pitEntry->ClearIncoming ();

      // Remove also outgoing
      pitEntry->ClearOutgoing ();

      // Set pruning timout on PIT entry (instead of deleting the record)
      m_pit->MarkErased (pitEntry);
    }
}



bool
ForwardingStrategy::DetectRetransmittedInterest (Ptr<Face> inFace,
                                                 Ptr<const Interest> header,
                                                 Ptr<const Packet> packet,
                                                 Ptr<pit::Entry> pitEntry)
{
  pit::Entry::in_iterator existingInFace = pitEntry->GetIncoming ().find (inFace);

  bool isRetransmitted = false;

  if (existingInFace != pitEntry->GetIncoming ().end ())
    {
      // this is almost definitely a retransmission. But should we trust the user on that?
      isRetransmitted = true;
    }

  return isRetransmitted;
}

void
ForwardingStrategy::SatisfyPendingInterest (Ptr<Face> inFace,
                                            Ptr<const ContentObject> header,
                                            Ptr<const Packet> payload,
                                            Ptr<const Packet> origPacket,
                                            Ptr<pit::Entry> pitEntry)
{
	bool local = true;
  if (inFace != 0)                         // The requested Data comes from another node, so the inFace must be removed from the pitEntry.
  {
	  pitEntry->RemoveIncoming (inFace);
	  local = false;
  }

  //satisfy all pending incoming Interests
  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
  {
     bool ok = incoming.m_face->Send (origPacket->Copy ());

     //NS_LOG_UNCOND("FS/SendingData - DATA:\t" << header->GetName() << "\tsent towards interface:\t" << incoming.m_face->GetId());


     TypeId faceType = incoming.m_face->GetInstanceTypeId();
	 std::string faceType_name = faceType.GetName();
	 if (ok)
     {
        if(faceType_name.compare("ns3::ndn::AppFace") == 0)
           m_inDataCacheApp (header, "RX", nodeType);
       	   // m_inDataCacheApp (header);

        else
//       	   m_outData (header, inFace == 0, incoming.m_face);
        {
        	if(local)  // The requested Data comes from the local cache
        		m_outData (header, incoming.m_face, "TX", nodeType);
        	else      // The requested Data comes from another node
        		m_outData (header, incoming.m_face, "FRW", nodeType);
        }
        DidSendOutData (inFace, incoming.m_face, header, payload, origPacket, pitEntry);

        NS_LOG_DEBUG ("Satisfy " << *incoming.m_face);
     }
     else
     {
        //m_dropData (header, incoming.m_face, dropped);
        //m_dropData (header, dropped);
        NS_LOG_DEBUG ("Cannot satisfy data to " << *incoming.m_face);
     }
  }

  // All incoming interests are satisfied. Remove them
  pitEntry->ClearIncoming ();

  // Remove all outgoing faces
  pitEntry->ClearOutgoing ();

  // Set pruning timout on PIT entry (instead of deleting the record)
  m_pit->MarkErased (pitEntry);
}

void
ForwardingStrategy::DidReceiveSolicitedData (Ptr<Face> inFace,
                                             Ptr<const ContentObject> header,
                                             Ptr<const Packet> payload,
                                             Ptr<const Packet> origPacket,
                                             bool didCreateCacheEntry)
{
  // do nothing
}

void
ForwardingStrategy::DidReceiveUnsolicitedData (Ptr<Face> inFace,
                                               Ptr<const ContentObject> header,
                                               Ptr<const Packet> payload,
                                               Ptr<const Packet> origPacket,
                                               bool didCreateCacheEntry)
{
  // do nothing
}

void
ForwardingStrategy::WillSatisfyPendingInterest (Ptr<Face> inFace,
                                                Ptr<pit::Entry> pitEntry)
{
  pit::Entry::out_iterator out = pitEntry->GetOutgoing ().find (inFace);

  // If we have sent interest for this data via this face, then update stats.
  if (out != pitEntry->GetOutgoing ().end ())
    {
      pitEntry->GetFibEntry ()->UpdateFaceRtt (inFace, Simulator::Now () - out->m_sendTime);
    }

  //m_satisfiedInterests (pitEntry);
}

bool
ForwardingStrategy::ShouldSuppressIncomingInterest (Ptr<Face> inFace,
                                                    Ptr<const Interest> header,
                                                    Ptr<const Packet> origPacket,
                                                    Ptr<pit::Entry> pitEntry)
{
  bool isNew = pitEntry->GetIncoming ().size () == 0 && pitEntry->GetOutgoing ().size () == 0;

  if (isNew) return false; // never suppress new interests

  bool isRetransmitted = m_detectRetransmissions && // a small guard
                         DetectRetransmittedInterest (inFace, header, origPacket, pitEntry);

  if (pitEntry->GetOutgoing ().find (inFace) != pitEntry->GetOutgoing ().end ())
    {
      NS_LOG_DEBUG ("Non duplicate interests from the face we have sent interest to. Don't suppress");
      // got a non-duplicate interest from the face we have sent interest to
      // Probably, there is no point in waiting data from that face... Not sure yet

      // If we're expecting data from the interface we got the interest from ("producer" asks us for "his own" data)
      // Mark interface YELLOW, but keep a small hope that data will come eventually.

      // ?? not sure if we need to do that ?? ...

      // pitEntry->GetFibEntry ()->UpdateStatus (inFace, fib::FaceMetric::NDN_FIB_YELLOW);
    }
  else
    if (!isNew && !isRetransmitted)
      {
        return true;
      }

  return false;
}

void
ForwardingStrategy::PropagateInterestQtab (Ptr<Face> inFace,
                                       Ptr<const Interest> header,
                                       Ptr<const Packet> origPacket,
                                       Ptr<pit::Entry> pitEntry,
                                       Ptr<qtab::Entry> qtabEntry)
{
  bool isRetransmitted = m_detectRetransmissions && // a small guard
                         DetectRetransmittedInterest (inFace, header, origPacket, pitEntry);

  pitEntry->AddIncoming (inFace/*, header->GetInterestLifetime ()*/);
  /// @todo Make lifetime per incoming interface
  //pitEntry->UpdateLifetime (header->GetInterestLifetime ());
  pitEntry->UpdateLifetime (Seconds(2.0));

  bool propagated = DoPropagateInterestQtab (inFace, header, origPacket, pitEntry, qtabEntry);

  if (!propagated && isRetransmitted) //give another chance if retransmitted
  {
    // increase max number of allowed retransmissions
    pitEntry->IncreaseAllowedRetxCount ();

    // try again
    propagated = DoPropagateInterestQtab (inFace, header, origPacket, pitEntry, qtabEntry);
  }

  // if (!propagated)
  //   {
  //     NS_LOG_DEBUG ("++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  //     NS_LOG_DEBUG ("+++ Not propagated ["<< header->GetName () <<"], but number of outgoing faces: " << pitEntry->GetOutgoing ().size ());
  //     NS_LOG_DEBUG ("++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  //   }

  // ForwardingStrategy will try its best to forward packet to at least one interface.
  // If no interests was propagated, then there is not other option for forwarding or
  // ForwardingStrategy failed to find it.
  //if (!propagated && pitEntry->AreAllOutgoingInVain ())
  if (!propagated && pitEntry->GetOutgoing ().size () == 0)
  {
	  if(inFace->GetNode()->GetId() == 0)
		  NS_LOG_UNCOND("Interest not Propagated!");

     DidExhaustForwardingOptions (inFace, header, origPacket, pitEntry);
     //m_qtab->MarkErasedQtab(qtabEntry);
     qtabEntry->DecrementExplorationChunks();
     //NS_LOG_UNCOND("NODE:\t" << inFace->GetNode()->GetId() << "\t PIT ENTRY MARK ERASED:\t" << pitEntry->GetPrefix() << "\t Time:\t" << Simulator::Now().GetNanoSeconds());
     //m_pit->MarkErased (pitEntry);
  }
}


void
ForwardingStrategy::PropagateInterest (Ptr<Face> inFace,
                                       Ptr<const Interest> header,
                                       Ptr<const Packet> origPacket,
                                       Ptr<pit::Entry> pitEntry)
{
  bool isRetransmitted = m_detectRetransmissions && // a small guard
                         DetectRetransmittedInterest (inFace, header, origPacket, pitEntry);

  pitEntry->AddIncoming (inFace/*, header->GetInterestLifetime ()*/);
  /// @todo Make lifetime per incoming interface
  //pitEntry->UpdateLifetime (header->GetInterestLifetime ());
  pitEntry->UpdateLifetime (Seconds(2.0));

  bool propagated = DoPropagateInterest (inFace, header, origPacket, pitEntry);

  if (!propagated && isRetransmitted) //give another chance if retransmitted
  {
    // increase max number of allowed retransmissions
    pitEntry->IncreaseAllowedRetxCount ();

    // try again
    propagated = DoPropagateInterest (inFace, header, origPacket, pitEntry);
  }

  // if (!propagated)
  //   {
  //     NS_LOG_DEBUG ("++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  //     NS_LOG_DEBUG ("+++ Not propagated ["<< header->GetName () <<"], but number of outgoing faces: " << pitEntry->GetOutgoing ().size ());
  //     NS_LOG_DEBUG ("++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  //   }

  // ForwardingStrategy will try its best to forward packet to at least one interface.
  // If no interests was propagated, then there is not other option for forwarding or
  // ForwardingStrategy failed to find it.
  //if (!propagated && pitEntry->AreAllOutgoingInVain ())
  if (!propagated && pitEntry->GetOutgoing ().size () == 0)
  {
     DidExhaustForwardingOptions (inFace, header, origPacket, pitEntry);
     //NS_LOG_UNCOND("NODE:\t" << inFace->GetNode()->GetId() << "\t PIT ENTRY MARK ERASED:\t" << pitEntry->GetPrefix() << "\t Time:\t" << Simulator::Now().GetMicroSeconds());
     //m_pit->MarkErased (pitEntry);
  }
}

// ** [MT] ** Modified PropagateInterest

void
ForwardingStrategy::PropagateInterest_NEW (Ptr<Face> inFace,
                                           Ptr<const Interest> header,
                                           Ptr<const Packet> origPacket,
                                           Ptr<pit::Entry> pitEntry)
{
  /*bool isRetransmitted = m_detectRetransmissions && // a small guard
                      DetectRetransmittedInterest (incomingFace, pitEntry);*/

  pitEntry->AddIncoming (inFace);
  /// @todo Make lifetime per incoming interface
  //pitEntry->UpdateLifetime (header->GetInterestLifetime ());
  pitEntry->UpdateLifetime (Seconds(2.0));


  //Ptr<Face> prevFace = (*pitEntry->GetOutgoing().rend()).m_face;

  bool propagated = DoPropagateInterest (inFace, header, origPacket, pitEntry);


//NS_LOG_UNCOND("NODO: " << inFace->GetNode()->GetId() << "\tPROPAGATED COUNT: " << propagated);
//  if (!propagated && isRetransmitted) //give another chance if retransmitted
//    {
//      // increase max number of allowed retransmissions
//      pitEntry->IncreaseAllowedRetxCount ();
//
//NS_LOG_UNCOND("NODE:\t" << inFace->GetNode()->GetId() << "\tNumber of retransmissions for this PIT:\t" << pitEntry->GetMaxRetxCount ());
//
//      // try again
//      propagated = DoPropagateInterest (incomingFace, header, packet, pitEntry);
//    }

  // ForwardingStrategy will try its best to forward packet to at least one interface.
  // If no interests was propagated, then there is not other option for forwarding or
  // ForwardingStrategy failed to find it.
  if (!propagated) //&& pitEntry->GetOutgoing ().size () == 0)
  {
    DidExhaustForwardingOptions (inFace, header, origPacket, pitEntry);
  }
}



bool
ForwardingStrategy::CanSendOutInterest (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<const Interest> header,
                                        Ptr<const Packet> origPacket,
                                        Ptr<pit::Entry> pitEntry)
{
  if (outFace == inFace)
    {
      // NS_LOG_DEBUG ("Same as incoming");
      return false; // same face as incoming, don't forward
    }

  pit::Entry::out_iterator outgoing =
    pitEntry->GetOutgoing ().find (outFace);

  if (outgoing != pitEntry->GetOutgoing ().end ())
    {
      if (!m_detectRetransmissions)
        return false; // suppress
      else if (outgoing->m_retxCount >= pitEntry->GetMaxRetxCount ())
        {
          // NS_LOG_DEBUG ("Already forwarded before during this retransmission cycle (" <<outgoing->m_retxCount << " >= " << pitEntry->GetMaxRetxCount () << ")");
          return false; // already forwarded before during this retransmission cycle
        }
   }

  return true;
}


bool
ForwardingStrategy::TrySendOutInterest (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<const Interest> header,
                                        Ptr<const Packet> origPacket,
                                        Ptr<pit::Entry> pitEntry)
{
  if (!CanSendOutInterest (inFace, outFace, header, origPacket, pitEntry))
    {
      return false;
    }

  pitEntry->AddOutgoing (outFace);

  //transmission
  Ptr<Packet> packetToSend = origPacket->Copy ();
  bool successSend = outFace->Send (packetToSend);
  if (!successSend)
    {
      m_dropInterests (header, outFace);
    }

  DidSendOutInterest (inFace, outFace, header, origPacket, pitEntry);

  return true;
}

// *** INFORM ***
bool
ForwardingStrategy::SendOutInterestInform (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<const Interest> header,
                                        Ptr<const Packet> origPacket,
                                        Ptr<pit::Entry> pitEntry)
{
  if (outFace == inFace)
  {
      // NS_LOG_DEBUG ("Same as incoming");
      return false; // same face as incoming, don't forward
  }

  pitEntry->AddOutgoing (outFace);

  //transmission
  Ptr<Packet> packetToSend = origPacket->Copy ();
  bool successSend = outFace->Send (packetToSend);
  if (!successSend)
    {
      m_dropInterests (header, outFace);
    }

  DidSendOutInterest (inFace, outFace, header, origPacket, pitEntry);

  return true;
}
//////////



void
ForwardingStrategy::DidSendOutInterest (Ptr<Face> inFace,
                                        Ptr<Face> outFace,
                                        Ptr<const Interest> header,
                                        Ptr<const Packet> origPacket,
                                        Ptr<pit::Entry> pitEntry)
{
  //m_outInterests (header, outFace);
  m_outInterests (header, outFace, "FRW", nodeType);
}

void
ForwardingStrategy::DidSendOutData (Ptr<Face> inFace,
                                    Ptr<Face> outFace,
                                    Ptr<const ContentObject> header,
                                    Ptr<const Packet> payload,
                                    Ptr<const Packet> origPacket,
                                    Ptr<pit::Entry> pitEntry)
{
//  m_outData (header, inFace == 0, outFace);
    //m_outData (header, inFace == 0, outFace, "Tx");
}

void
ForwardingStrategy::WillEraseTimedOutPendingInterest (Ptr<pit::Entry> pitEntry)
{
  //m_timedOutInterests (pitEntry);
  std::stringstream ss;
  pitEntry->GetPrefix().Print(ss);
  std::string name = ss.str();
  Ptr<Name> nameCom = Create<Name> (name);
  Ptr<Interest> header = Create<Interest> ();
  header->SetName(nameCom);
  m_expiredEntry(header);
  ss.str("");
}

void
ForwardingStrategy::AddFace (Ptr<Face> face)
{
  // do nothing here
}

void
ForwardingStrategy::RemoveFace (Ptr<Face> face)
{
  // do nothing here
}

void
ForwardingStrategy::DidAddFibEntry (Ptr<fib::Entry> fibEntry)
{
  // do nothing here
}

void
ForwardingStrategy::WillRemoveFibEntry (Ptr<fib::Entry> fibEntry)
{
  // do nothing here
}

std::ifstream& ForwardingStrategy::GotoLine(std::ifstream& file, uint32_t num)
{
   file.seekg(std::ios::beg);
   for(uint32_t i=0; i < num - 1; ++i)
   {
       file.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
   }
   return file;
}


} // namespace ndn
} // namespace ns3

