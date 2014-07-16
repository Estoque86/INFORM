/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
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
 * Author: Ilya Moiseenko <iliamo@cs.ucla.edu>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/point-to-point-grid.h"
#include "ns3/mobility-module.h"
#include "../src/ndnSIM/helper/ndn-global-routing-helper.h"
#include "../src/ndnSIM/model/ndn-global-router.h"
#include "../src/ndnSIM/model/qtab/ndn-qtab.h"
#include <string.h>
#include <stdio.h>
#include <sstream>
#include <fstream>
#include "ns3/random-variable-stream.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <boost/tokenizer.hpp>
#include <boost/ref.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>


using namespace ns3;
using namespace ndn;

NS_LOG_COMPONENT_DEFINE ("ndn.NewRouting");

std::vector<std::vector<bool> > readAdjMat (std::string adj_mat_file_name);
std::vector<std::vector<double> > readCordinatesFile (std::string node_coordinates_file_name);


// ***  FUNZIONI PER IL NUOVO TRACING
void InterestTrace(Ptr<OutputStreamWrapper> stream, Ptr<const Interest> header, Ptr<const Face> face, std::string eventType, std::string nodeType);
void DataTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, Ptr<const Face> face, std::string eventType, std::string nodeType);
void DataAppTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, std::string eventType, std::string nodeType);
void InterestAppTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, std::string eventType, std::string nodeType);
void DownloadTimeTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, int64_t downloadTime, uint32_t dist, std::string eventType, std::string nodeType);


int
main (int argc, char *argv[14])
{
  struct rlimit newlimit;
  const struct rlimit * newlimitP;
  newlimitP = NULL;
  newlimit.rlim_cur = 2000;
  newlimit.rlim_max = 2000;
  newlimitP = &newlimit;

  setrlimit(RLIMIT_NOFILE,newlimitP);

  static ns3::GlobalValue g_simRun ("g_simRun",
                                   "The number of the current simulation in the same scenario",
                                   ns3::UintegerValue(1),
                                   ns3::MakeUintegerChecker<uint32_t> ());

  static ns3::GlobalValue g_alphaValue ("g_alphaValue",
                                   "Alpha - Zipf's Parameter",
                                   ns3::DoubleValue(1),
                                   ns3::MakeDoubleChecker<uint32_t> ());



  // ** [MT] ** Parameters passed with command line
  uint64_t uniqueContents = 0;               // Exact Number of Unique contents
  uint64_t contentCatalogFib = 0;            // Estimated Content Catalog Cardinality to dimension the Bloom Filter for the FIB.
  double cacheToCatalog = 0;                 // The cacheSize/contentCatalogFib ratio.
  double alpha = 1;			     // Zipf's Exponent
  uint32_t lambda = 1;			     // Request rate per each client
  uint32_t clientPerc = 50;    		     // Percentage of core nodes with an attached client.
  double eta = 0;			     // Eta parameter to calculate the Q-value
  double delta = 0;			     // Threshold indicating when the Exploitation phase must be stopped
  uint32_t maxChunkExplorPhase = 0;	     // Max num of chunk for the Exploration phase.
  uint32_t maxChunkExploitPhase = 0;	     // Max num of chunk for the Exploitation phase.
  uint32_t qTabEntryLifetime = 0; 	     // Lifetime for the qTab Entry [s]
  std::string networkType = "";              // Type of simulated network (Network Name)
  std::string topologyImport = "";	     // How to create the network (Annotated or Adjacency)


  double simDuration = 200.0;                                    // Duration of the Simulation [s].

  CommandLine cmd;
  cmd.AddValue ("uniqueContents", "Real Content Catalog Cardinality", uniqueContents);
  cmd.AddValue ("contentCatalogFib", "Estimated Content Catalog Cardinality for the Bloom Filter for the Fib", contentCatalogFib);
  cmd.AddValue ("cacheToCatalog", "Cache to Catalog Ratio", cacheToCatalog);
  cmd.AddValue ("alpha", "Zipf's Parameter", alpha);
  cmd.AddValue ("lambda", "Request Rate", lambda);
  cmd.AddValue ("clientPerc", "Percentage of Core Nodes with an attached Client", clientPerc);
  cmd.AddValue ("eta", "Weight of the average for Q-value calculation", eta);
  cmd.AddValue ("delta", "Threshold related to the exploitation phase", delta);
  cmd.AddValue ("maxChunkExplorPhase", "Maximum number of chunks for the exploration phase", maxChunkExplorPhase);
  cmd.AddValue ("maxChunkExploitPhase", "Maximum number of chunks for the exploitation phase", maxChunkExploitPhase);
  cmd.AddValue ("qTabEntryLifetime", "Maximum amount of time with no Interests before deleting the relative entry", qTabEntryLifetime);
  cmd.AddValue ("networkType", "Type of Simulated Network", networkType);
  cmd.AddValue ("topologyImport", "How to create the topology (Annotated or Adjacency)", topologyImport);
  cmd.AddValue ("simDuration", "Duration of the Simulation", simDuration);


  cmd.Parse (argc, argv);

  Time finishTime = Seconds (simDuration);

  uint64_t simRun = SeedManager::GetRun();
  std::stringstream ss;
  std::string simRunStr;
  ss << simRun;
  simRunStr = ss.str();
  ss.str("");


  Config::SetGlobal ("g_simRun", UintegerValue(simRun));
  Config::SetGlobal ("g_alphaValue", DoubleValue(alpha));

  // ** [MT] ** Calculate the cache size according to the specified ratio.
  uint32_t cacheSize = round((double)contentCatalogFib * cacheToCatalog);
  ss << cacheSize;
  std::string cacheSizeStr = ss.str();
  ss.str("");

  // *** Obtaining general parameter strings
  std::string alphaStr;
  ss << alpha;
  alphaStr = ss.str();
  ss.str("");

  //uint32_t plateau = 0;
  std::string plateauStr = "0";

  std::string catalogCardinalityStr;
  ss << contentCatalogFib;
  catalogCardinalityStr = ss.str();
  ss.str("");

  std::string realCardinalityStr;
  ss << uniqueContents;
  realCardinalityStr = ss.str();
  ss.str("");

  std::string lambdaStr;
  ss << lambda;
  lambdaStr = ss.str();
  ss.str("");

  std::string nrStr;
  ss << maxChunkExplorPhase;
  nrStr = ss.str();
  ss.str("");

  std::string ntStr;
  ss << maxChunkExploitPhase;
  ntStr = ss.str();
  ss.str("");

  std::string EXT = ".txt";
  std::string stringScenario;  		// Estensione per distinguere i vari scenari simulati


  std::string simulationType = "Inform";


  ss << "_S=" << simulationType << "_N=" << networkType << "_eta=" << eta << "_delta=" << delta << "_chunkExplor=" << maxChunkExplorPhase << "_chunkExploit=" << maxChunkExploitPhase << "_qTabLife=" << qTabEntryLifetime << "_A=" << alpha << "_R=" << SeedManager::GetRun();
  stringScenario = ss.str();
  ss.str("");


  // *******************************************

  // ***********  TOPOLOGY CREATION  ************

  uint32_t numCoreNodes;
  std::string topoPath;
  NodeContainer coreNodes, repoNodes, clientNodes;

  AnnotatedTopologyReader topologyReader ("", 10);

  std::string linkRateCore ("10Gbps");
  std::string linkDelayCore ("5000us");
  std::string txBuffer("40");

  std::string linkRateEdge ("1Gbps");      // Links connecting core with repos and/or clients
  std::string linkDelayEdge ("15000us");

  if(topologyImport.compare("Annotated")==0)
  {
	  const char* topoPathCh;
	  ss << "../TOPOLOGIES/" << topologyImport << "/" << networkType << ".txt";
	  topoPathCh = ss.str().c_str();
	  ss.str("");
	  //topologyReader.SetFileName ("../TOPOLOGIES/Geant_Topo_Only.txt");
	  topologyReader.SetFileName (topoPathCh);
	  topologyReader.Read ();
	  numCoreNodes = topologyReader.GetNodes().GetN();
  }
  else if(topologyImport.compare("Adjacency")==0)
  {
	  // Creazione con lettura da matrice di adiacenza..settare numero totale nodi.

	  ss << "../TOPOLOGIES/" << topologyImport << "/" << networkType << ".txt";
	  topoPath = ss.str();
	  ss.str("");

	  std::vector<std::vector<bool> > adjMatrix = readAdjMat (topoPath);
	  numCoreNodes = adjMatrix.size();

	  coreNodes.Create(numCoreNodes);

	  // NB: Si potrebbe aggiungere una procedura di selezione random dove vengono settati gli
	  //     Error Rate di alcuni link.

	  PointToPointHelper p2p;
	  p2p.SetDeviceAttribute ("DataRate", StringValue (linkRateCore));
	  p2p.SetChannelAttribute ("Delay", StringValue (linkDelayCore));

	  // Create Links between Nodes
	  uint32_t linkCount = 0;

	  for (size_t i = 0; i < adjMatrix.size (); i++)
	  {
		  for (size_t j = 0; j < adjMatrix[i].size (); j++)
	      {
			  if (adjMatrix[i][j] == 1)
	          {
		          // Create Links between Core Nodes and Repos
		          uint32_t linkCountRepos = 0;

		          Ptr<Node> one;
		          Ptr<Node> two;

		          one = coreNodes.Get (i);
		          two = coreNodes.Get (j);

		          AnnotatedTopologyReader::Link link (one, "core_1", two, "core_2");
		          link.SetAttribute("DataRate", linkRateCore);
		          link.SetAttribute("OSPF", "1");
		          link.SetAttribute("Delay", linkDelayCore);
		          link.SetAttribute ("MaxPackets", txBuffer);
		          topologyReader.AddLink(link);

		          //NetDeviceContainer n_devs = p2p.Install (n_links);
		          linkCount++;

				  //#######################################################
//				  NodeContainer n_links = NodeContainer (coreNodes.Get (i), coreNodes.Get (j));
//	              NetDeviceContainer n_devs = p2p.Install (n_links);
//	              linkCount++;
//	              // NB: Forse servirˆ settare gli attributi di Link della classe TopologyReader (tipo FromNode, ToNode)
//
//	              PointerValue txQueue;
//
//	              n_devs.Get(0)->GetAttribute ("TxQueue", txQueue);
//	              NS_ASSERT (txQueue.Get<DropTailQueue> () != 0);
//	              txQueue.Get<DropTailQueue> ()->SetAttribute ("MaxPackets", StringValue (txBuffer));
//
//	              n_devs.Get(1)->GetAttribute ("TxQueue", txQueue);
//	              NS_ASSERT (txQueue.Get<DropTailQueue> () != 0);
//	              txQueue.Get<DropTailQueue> ()->SetAttribute ("MaxPackets", StringValue (txBuffer));
	          }
	      }
	  }

	  NS_LOG_INFO ("Number of links in the adjacency matrix is: " << linkCount);
  }
  else
  {
	  std::cout << "Please choose between \"Annotated or Adjacency\" to create the topology!";
	  exit(1);
  }

  // ** Total number of nodes in the simulated topology
  //uint32_t numNodes = topologyReader.GetNodes().GetN();
  NS_LOG_UNCOND("The total number of core nodes is:\t" << numCoreNodes);
  
  // ******************************************************

  // **********  REPOSITORY RANDOM PLACEMENT  ******************

  // Both the number of repositories and their attachments are extracted casually.
  uint32_t maxNumRepo = 3;
  UniformVariable m_SeqRngRepoAttach (0, numCoreNodes-1);     // The maximum random number is equal to the number of core nodes
  UniformVariable m_SeqRngNumRepo (1, maxNumRepo);

  bool completeRepo = false;

  uint32_t numRandRepo = (uint32_t)round(m_SeqRngNumRepo.GetValue());

  std::vector<uint32_t>* repoAttachesID = new std::vector<uint32_t>(numRandRepo) ;
  for(uint32_t i=0; i<repoAttachesID->size(); i++)
  {
	  repoAttachesID->operator[](i) = numCoreNodes+1;
  }


  uint32_t repoAttachNum = 0;
  uint32_t randRepoAttach;

  while(!completeRepo)
  {
	  bool already_extracted = false;
	  while(!already_extracted)
	  {

		  randRepoAttach = (uint32_t)round(m_SeqRngRepoAttach.GetValue());
		  for(uint32_t i=0; i<repoAttachesID->size(); i++)
		  {
			  if(repoAttachesID->operator[](i)==randRepoAttach)
			  {
				already_extracted = true;
				break;
			  }
		  }
		  if(already_extracted==true)
			already_extracted = false;        // It keeps going
		  else
			already_extracted = true;
	  }
	  repoAttachesID->operator[](repoAttachNum) = randRepoAttach;
	  NS_LOG_UNCOND("CHOSEN REPOSITORY ATTACHMENT:\t" << repoAttachesID->operator[](repoAttachNum));
	  repoAttachNum = repoAttachNum + 1;
	  if(repoAttachNum == repoAttachesID->size())
		completeRepo = true;
  }

  // Creation and Attachment of Repo Nodes

  repoNodes.Create(numRandRepo);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (linkRateEdge));
  p2p.SetChannelAttribute ("Delay", StringValue (linkDelayEdge));

  for(uint32_t i=0; i<repoAttachesID->size(); i++) 
  {
          // Create Links between Core Nodes and Repos
          uint32_t linkCountRepos = 0; 

          Ptr<Node> attach;
          Ptr<Node> repo;

          //NodeContainer n_links;
          if(topologyImport.compare("Annotated")==0)
          {    
                  //n_links = NodeContainer (topologyReader.GetNodes().Get (repoAttachesID->operator[](i)), repoNodes.Get (i));
                  attach  = topologyReader.GetNodes().Get (repoAttachesID->operator[](i));
                  repo = repoNodes.Get (i); 
          }    
          else 
          {    
                  //n_links = NodeContainer (coreNodes.Get (repoAttachesID->operator[](i)), repoNodes.Get (i));
                  attach = coreNodes.Get (repoAttachesID->operator[](i));
                  repo = repoNodes.Get (i); 
          }    

          uint32_t idAttach = attach->GetId();
          uint32_t idRepo = repo->GetId();

	  NS_LOG_UNCOND("ID ATTACH: " << idAttach << "\tID REPO: " << idRepo);

          AnnotatedTopologyReader::Link link (attach, "core_2", repo, "repo1");
          link.SetAttribute("DataRate", linkRateEdge);
          link.SetAttribute("OSPF", "1");
          link.SetAttribute("Delay", linkDelayEdge);
          link.SetAttribute ("MaxPackets", txBuffer);
          topologyReader.AddLink(link);

          //NetDeviceContainer n_devs = p2p.Install (n_links);
          linkCountRepos++;
  }

  // Calculate the Repo Size according to the number of extracted repos
  uint32_t repoSize;
  std::string repoSizeStr;
  std::string pathRepo;

  switch(numRandRepo)
  {
  case(1):
                repoSize = uniqueContents;
        ss << repoSize;
                repoSizeStr = ss.str();
                ss.str("");
                pathRepo= "../SEED_COPIES/CommonCrawl/CommonCrawl-NEW_1_1.txt";       // One Repo stores all the contents.
            break;
  case(2):
                repoSize = round(uniqueContents/2) + 1;
        ss << repoSize;
                repoSizeStr = ss.str();
                ss.str("");
                pathRepo= "../SEED_COPIES/CommonCrawl/CommonCrawl-NEW_2_";            // Two Repos
                break;
  case(3):
                //RepoSize = round(contentCatalog/3) + 3;
        repoSize = round(uniqueContents/3) + 3;
        ss << repoSize;
                repoSizeStr = ss.str();
                ss.str("");
                pathRepo= "../SEED_COPIES/CommonCrawl/CommonCrawl-NEW_3_";            // Three Repos
                break;
  default:
          NS_LOG_UNCOND("Number of Repos not supported!\t" << numRandRepo);
          exit(1);
          break;
  }

  // ************************************************************

  // *************   CLIENT RANDOM PLACEMENT   *******************

  // Both the number of clients and their attachments are extracted casually.
  uint32_t numClients = ceil((double)((numCoreNodes-numRandRepo)/100.)*clientPerc);     // 60% of the core nodes has a client attached

  NS_LOG_UNCOND("Numero CLIENT PERC: " << clientPerc);
  NS_LOG_UNCOND("Numero CORE NODES: " << numCoreNodes);
  NS_LOG_UNCOND("Numero RAND REPOS: " << numRandRepo);
  NS_LOG_UNCOND("Numero CLIENT: " << numClients);

  bool completeClients = false;

  std::vector<uint32_t>* clientAttachesID = new std::vector<uint32_t>(numClients) ;
  for(uint32_t i=0; i<clientAttachesID->size(); i++)
  {
	  clientAttachesID->operator[](i) = numCoreNodes+1;
  }


  uint32_t clientAttachNum = 0;
  uint32_t randClientAttach;

  while(!completeClients)
  {
	  bool already_extracted = false;
	  while(!already_extracted)
	  {

		  randClientAttach = (uint32_t)round(m_SeqRngRepoAttach.GetValue());
		  NS_LOG_UNCOND("Rand Client Attach: " << randClientAttach);

		  for(uint32_t i=0; i<clientAttachesID->size(); i++)
		  {
			  if(clientAttachesID->operator[](i)==randClientAttach)
			  {
				already_extracted = true;
				break;
			  }
			  for(uint32_t j=0; j<repoAttachesID->size(); j++)
			  {
				  if(repoAttachesID->operator[](j)==randClientAttach)
				  {
					  already_extracted = true;
					  break;
				  }
			  }
		  }
		  if(already_extracted==true)
			already_extracted = false;        // It keeps going
		  else
			already_extracted = true;
	  }
	  clientAttachesID->operator[](clientAttachNum) = randClientAttach;
	  NS_LOG_UNCOND("CHOSEN CLIENT ATTACHMENT:\t" << clientAttachesID->operator[](clientAttachNum));
	  clientAttachNum = clientAttachNum + 1;
	  if(clientAttachNum == clientAttachesID->size())
		completeClients = true;
  }

  // Creation and Attachment of Client Nodes

  clientNodes.Create(numClients);

  //PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (linkRateEdge));
  p2p.SetChannelAttribute ("Delay", StringValue (linkDelayEdge));

  for(uint32_t i=0; i<clientAttachesID->size(); i++)
  {
          // Create Links between Core Nodes and Repos
          uint32_t linkCountClients = 0;

          Ptr<Node> attachClient;
          Ptr<Node> client;

          //NodeContainer n_links;
          if(topologyImport.compare("Annotated")==0)
          {
                  //n_links = NodeContainer (topologyReader.GetNodes().Get (repoAttachesID->operator[](i)), repoNodes.Get (i));
                  attachClient  = topologyReader.GetNodes().Get (clientAttachesID->operator[](i));
                  client = clientNodes.Get (i);
          }
          else
          {
                  //n_links = NodeContainer (coreNodes.Get (repoAttachesID->operator[](i)), repoNodes.Get (i));
                  attachClient = coreNodes.Get (clientAttachesID->operator[](i));
                  client = clientNodes.Get (i);
          }

          AnnotatedTopologyReader::Link link (attachClient, "core_1", client, "client");
          link.SetAttribute("DataRate", linkRateEdge);
          link.SetAttribute("OSPF", "1");
          link.SetAttribute("Delay", linkDelayEdge);
          link.SetAttribute ("MaxPackets", txBuffer);
          topologyReader.AddLink(link);

          //NetDeviceContainer n_devs = p2p.Install (n_links);
          linkCountClients++;
  }



  // *******************************************

  // **********  INSTALL THE NDN STACK   ****************

  NS_LOG_INFO ("Installing Ndn stack on all nodes");
  ndn::StackHelper ndnHelper;
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;                           // ****** ROUTING HELPER


  ndnHelper.SetForwardingStrategy("ns3::ndn::fw::FloodingInform");     // Selective flooding for BF scenario
  ndnHelper.SetDefaultRoutes(true);


//  NodeContainer producerNodes;
//  NodeContainer consumerNodes;

  std::vector<uint32_t>* reposID = new std::vector<uint32_t>(numRandRepo) ;

  uint32_t i = 0;
  for (NodeContainer::Iterator nodeRepo = repoNodes.Begin(); nodeRepo != repoNodes.End(); ++nodeRepo)
  {
	  reposID->operator[](i) = (*nodeRepo)->GetId();
 	  i++;
  }

  topologyReader.ApplySettingsCall();

  for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node ++)
  {
	  bool rep = false;
	  for (uint32_t j = 0; j < numRandRepo; j++)
	  {
		  if ((*node)->GetId() == reposID->operator [](j))
			  rep = true;
	  }
	  if (rep)
	  {
		  // **** For Repo nodes: Cache Size = 0, that is they store only permanent copies.
		  ndnHelper.SetContentStore("ns3::ndn::cs::Lru", "MaxSize", "0");
		  ndnHelper.SetRepository("ns3::ndn::rp::Persistent", "MaxSize", repoSizeStr);
	  }
	  else
	  {
		  // **** For Clients or Routers: Repo Size = 0, that is they do not store permanent copies.
		  ndnHelper.SetContentStore("ns3::ndn::cs::Lru", "MaxSize", cacheSizeStr);
		  ndnHelper.SetRepository("ns3::ndn::rp::Persistent", "MaxSize", "0");
		  //consumerNodes.Add((*node));
	  }
      ndnHelper.Install((*node));
  }


  topologyReader.ApplyDelayMetric ();   // Best Routes are evaluated accoridng to the Min Delay.

  ndnGlobalRoutingHelper.InstallAll ();


  // ********* Initialize Repos with SEED copies. Content names are read from files.

  std::string line;
  Ptr<Node> nd;
  const char *pathRepoCompl;
  std::ifstream fin;

  switch (numRandRepo)
  {
  case(1):

	// ****  There is only one repo.
	nd = NodeList::GetNode(reposID->operator[](0));
  	pathRepoCompl = pathRepo.c_str();

  	fin.open(pathRepoCompl);
  	if(!fin) {
  	   	std::cout << "\nERROR: Impossible to read for the file of contents!\n";
  	   	exit(0);
  	}

  	// **** Read seed copies from file and insert them inside the repo.
  	while(std::getline(fin, line))
  	{
  		static ContentObjectTail tail;
 	    Ptr<ContentObject> header = Create<ContentObject> ();
 	    header->SetName (Create<Name>(line));
 	    header->SetTimestamp (Simulator::Now ());
 	    header->SetSignature(0);
 	    Ptr<Packet> packet = Create<Packet> (512);
 	    packet->AddHeader (*header);
 	    packet->AddTrailer (tail);

 	    ndnGlobalRoutingHelper.AddOrigin (line, nd);

 	    nd->GetObject<Repo> ()->Add (header, packet);
  	}
  	fin.close();
  	break;

  case(2):
  case(3):

    // ****  There is more than one Repos
    for(uint32_t i = 0; i < reposID->size(); i++)
    {
       nd = NodeList::GetNode(reposID->operator[](i));
       ss << pathRepo << (i+1) << EXT;
  	   pathRepoCompl = ss.str().c_str();
       ss.str("");

       fin.open(pathRepoCompl);
  	   if(!fin) {
  	       	std::cout << "\nERROR: Impossible to read from the file of Contents!\n" << pathRepoCompl << "\t" << reposID->size() << "\n";
  	       	exit(0);
  	   }

  	   // **** Read seed copies from file and insert them inside the repo.
  	   while(std::getline(fin, line))
  	   {
           static ContentObjectTail tail;
  		   Ptr<ContentObject> header = Create<ContentObject> ();
  		   header->SetName (Create<Name>(line));
  		   header->SetTimestamp (Simulator::Now ());
  		   header->SetSignature(0);

  		   Ptr<Packet> packet = Create<Packet> (512);
  		   packet->AddHeader (*header);
  		   packet->AddTrailer (tail);

  		   ndnGlobalRoutingHelper.AddOrigin (line, nd);

  		   nd->GetObject<Repo> ()->Add (header, packet);
  		}
  		fin.close();
     }
     break;
  }

  ndnGlobalRoutingHelper.CalculateRoutes ();

NS_LOG_UNCOND("CHECK!!");

  // ****  Install Ndn Application Layers
  NS_LOG_INFO ("Installing Applications");
  std::string prefix = "/prefix";

  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
  consumerHelper.SetPrefix (prefix);
  //consumerHelper.SetAttribute ("Frequency", StringValue ("1"));

  consumerHelper.SetAttribute ("Frequency", StringValue (lambdaStr)); // lambda Interest per second
  consumerHelper.SetAttribute ("NumberOfContentsZipf", StringValue (realCardinalityStr));
  //consumerHelper.SetAttribute ("NumberOfContentsZipf", StringValue (catalogCardinalityStr));
  //consumerHelper.SetAttribute ("NumberOfContentsZipf", StringValue ("3"));
  //consumerHelper.SetAttribute ("Randomize", StringValue ("none"));
  consumerHelper.SetAttribute ("qZipf", StringValue (plateauStr)); // q paremeter
  consumerHelper.SetAttribute ("sZipf", StringValue (alphaStr)); // Zipf's exponent


  //consumerHelper.SetAttribute ("StartTime", TimeValue (Seconds (2.000000)));
  ApplicationContainer consumers = consumerHelper.Install (clientNodes);
  consumers.Start (Seconds(2));
  //consumers.Stop(finishTime-Seconds(1));


  // **** Settaggio della tipologia di nodo.

  for (NodeContainer::Iterator node_app = clientNodes.Begin(); node_app != clientNodes.End(); ++node_app)
  {
          (*node_app)->GetObject<ForwardingStrategy>()->SetNodeType("client");
  }

  for (NodeContainer::Iterator node_prod = repoNodes.Begin(); node_prod != repoNodes.End(); ++node_prod)
  {
          (*node_prod)->GetObject<ForwardingStrategy>()->SetNodeType("producer");
  }

  if(topologyImport.compare("Annotated")==0)
  {
        uint32_t numCoreNodes = topologyReader.GetNodes().GetN();
        for (uint32_t i = 0; i < numCoreNodes; i++)
        {
            Ptr<Node> node_core = topologyReader.GetNodes().Get(i);

            node_core->GetObject<ForwardingStrategy>()->SetNodeType("core");
        }
  }
  else
  {
          for (NodeContainer::Iterator node_core = coreNodes.Begin(); node_core != coreNodes.End(); ++node_core)
          {
                  (*node_core)->GetObject<ForwardingStrategy>()->SetNodeType("core");
          }
  }


  // ***** INFORM INITIALIZATION*****
  // Per each n de, qTab parameters are initialized: e.g., the vector conteining the rtt associated to each neighbor, etc...
  uint32_t numDev;
  Ptr<Qtab> qTabPointer;

  for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node ++)
  {
	  numDev = (*node)->GetNDevices();
	  qTabPointer = (*node)->GetObject<Qtab> ();
	  qTabPointer->InitializeRttVect(numDev);       // Initialization of the vector.
	  for(uint32_t i = 0; i < numDev; i++)
	  {
		  // ** Retrieve the Delay associated with the p2p link of a single interface
		  Ptr<NetDevice> ndFace = (*node)->GetDevice(i);
		  Ptr<PointToPointNetDevice> nd1 = ndFace->GetObject<PointToPointNetDevice> ();
		  Ptr<Channel> channel = nd1->GetChannel ();
		  Ptr<PointToPointChannel> ppChannel = DynamicCast<PointToPointChannel> (channel);
		  TimeValue metricDelay;
		  ppChannel->GetAttribute("Delay", metricDelay);
		  Time md = metricDelay.Get();

		  // ** Calcolo e assegno l'rtt associato.
		  int64_t mdInt = static_cast<int64_t> (md.ToInteger(Time::US));
		  mdInt =  2*mdInt;
		  Time rtt = MicroSeconds(mdInt);
		  qTabPointer->SetRttVect(rtt, i);

		  // ** Settaggio di ulteriori parametri.
		  qTabPointer->SetEta(eta);
		  qTabPointer->SetDelta(delta);
		  qTabPointer->SetMaxChunksExploration(maxChunkExplorPhase);
		  qTabPointer->SetMaxChunksExploitation(maxChunkExploitPhase);
		  Time expTime = Seconds(qTabEntryLifetime);
		  qTabPointer->SetQtabEntryLifetime(expTime);
//		    NS_LOG_UNCOND("DELAY NODO A1:\t" << boost::lexical_cast<unsigned short>(md.GetMilliSeconds()));
	  }

	  // ** Check the inserted rtt values
	  NS_LOG_UNCOND("MAIN - Check the inserted RTT values at Node:\t" << (*node)->GetId() << "\n");
	  for(uint32_t i = 0; i < numDev; i++)
	  {
		  NS_LOG_UNCOND("Interface:\t" << i << "\twith RTT=\t" << qTabPointer->GetRttVect().operator [](i) << " us\n");
	  }

  }





  // ************  NEW TRACING SYSTEM  **********************

  // Output Layout:      Time[us]       EventType       NodeType        ContentID

  //**** INTEREST
  /*
   * - [ FRW and RX ] Inoltrati e Ricevuti da Cache (dove per "Cache" si intendono le cache dei nodi client)
   * - [ FRW and RX ] Inoltrati e Ricevuti da Core
   * - [ RX ] Ricevuti da Repo
   * - [ AGGR ] Aggregati ai nodi Core
   *
   */
  std::string fnInterestClient;
  std::string fnInterestProducer;
  std::string fnInterestCore;

  //**** DATA
  /*
   * - [ TX ] Generati da Repo, da cache di nodi Core o Client
   * - [ FRW ] Inoltrati da nodi Core  (ad ogni FRW <8f> associato un RX)
   *
   */
  std::string fnDataClient;
  std::string fnDataProducer;
  std::string fnDataCore;

  //**** DATA RICEVUTI APP (Tracing a livello Applicativo)
  /*
   * - [ RX ] Ricevuti da nodi Client (compresi DATA provenienti da cache locale)
   */
  std::string fnDataAppClient;

  //**** INTEREST APP (Tracing a livello Applicativo)
  std::string fnInterestAppClient;
  /*
   * - [ TX ] Generati a livello applicativo da nodi client
   * - [ RTX ] Ritrasmessi da nodi client
   * - [ ELM ] Eliminati perch<8f> raggiunto il max numero di ritrasmissioni
   * - [ ELMFILE ] File scartato perch<8f> incompleto
   *
   * Output Layout:  Time[us]   TimeSent[us]    EventType       NodeType        ContentID
   */

  //**** DOWNLOAD TIME (Tracing a livello Applicativo)
  /*
   * - [ FIRST ] Seek Time
   * - [ FILE ] File Download Time
   *
   */
  std::string fnDwnTime;


  std::stringstream fname;
  AsciiTraceHelper asciiTraceHelper;

  uint32_t z = 0;

  //  ***** CLIENT
  for (NodeContainer::Iterator node = clientNodes.Begin(); node != clientNodes.End(); ++node)
  {
          // ******* INTEREST
          ss << "RESULTS/" <<  simulationType << "/INTEREST/InterestCLIENT_" << z << stringScenario;
          fnInterestClient = ss.str();
          const char *filename_interestClient = fnInterestClient.c_str();
          ss.str("");

          // ******* DATA
          ss << "RESULTS/" <<  simulationType << "/DATA/DataCLIENT_" << z << stringScenario;
          fnDataClient = ss.str();
          const char *filename_dataClient = fnDataClient.c_str();
          ss.str("");


          // ******** STREAM DI OUTPUT ***************
          Ptr<OutputStreamWrapper> streamInterest = asciiTraceHelper.CreateFileStream(filename_interestClient);
          Ptr<OutputStreamWrapper> streamData = asciiTraceHelper.CreateFileStream(filename_dataClient);

           // ***** ASSOCIAZIONE ALLE FUNZIONI CHE TRATTANO I VARI EVENTI DI TRACE *****
          (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("OutInterests", MakeBoundCallback(&InterestTrace, streamInterest));
          (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InInterests", MakeBoundCallback(&InterestTrace, streamInterest));
          (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("AggregateInterests", MakeBoundCallback(&InterestTrace, streamInterest));
          (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("OutData", MakeBoundCallback(&DataTrace, streamData));
          (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InData", MakeBoundCallback(&DataTrace, streamData));

          z = z+1;
  }

  z = 0;


  //  ***** PRODUCER
  for (NodeContainer::Iterator node = repoNodes.Begin(); node != repoNodes.End(); ++node)
  {
          // ******* INTEREST
          ss << "RESULTS/" <<  simulationType << "/INTEREST/InterestPRODUCER_" << z << stringScenario;
          fnInterestProducer = ss.str();
          const char *filename_interestProducer = fnInterestProducer.c_str();
          ss.str("");

          // ******* DATA
          ss << "RESULTS/" <<  simulationType << "/DATA/DataPRODUCER_" << z << stringScenario;
          fnDataProducer = ss.str();
          const char *filename_dataProducer = fnDataProducer.c_str();
          ss.str("");

          // ******** STREAM DI OUTPUT ***************
          Ptr<OutputStreamWrapper> streamInterest = asciiTraceHelper.CreateFileStream(filename_interestProducer);
          Ptr<OutputStreamWrapper> streamData = asciiTraceHelper.CreateFileStream(filename_dataProducer);

           // ***** ASSOCIAZIONE ALLE FUNZIONI CHE TRATTANO I VARI EVENTI DI TRACE *****
          (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("OutInterests", MakeBoundCallback(&InterestTrace, streamInterest));
          (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InInterests", MakeBoundCallback(&InterestTrace, streamInterest));
          (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("AggregateInterests", MakeBoundCallback(&InterestTrace, streamInterest));
          (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("OutData", MakeBoundCallback(&DataTrace, streamData));
          (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InData", MakeBoundCallback(&DataTrace, streamData));

          z = z+1;
  }


  // ***** CORE
  if(topologyImport.compare("Annotated")==0)
  {
        z = 0;
        uint32_t numCoreNodes = topologyReader.GetNodes().GetN();
        for (uint32_t i = 0; i < numCoreNodes; i++)
        {
                  // ******* INTEREST
                  ss << "RESULTS/" <<  simulationType << "/INTEREST/InterestCORE_" << z << stringScenario;
                  fnInterestCore = ss.str();
                  const char *filename_interestCore = fnInterestCore.c_str();
                  ss.str("");

                  // ******* DATA
                  ss << "RESULTS/" <<  simulationType << "/DATA/DataCORE_" << z << stringScenario;
                  fnDataCore = ss.str();
                  const char *filename_dataCore = fnDataCore.c_str();
                  ss.str("");


                  Ptr<Node> node = topologyReader.GetNodes().Get(i);

                  // ******** STREAM DI OUTPUT ***************
                  Ptr<OutputStreamWrapper> streamInterest = asciiTraceHelper.CreateFileStream(filename_interestCore);
                  Ptr<OutputStreamWrapper> streamData = asciiTraceHelper.CreateFileStream(filename_dataCore);

                   // ***** ASSOCIAZIONE ALLE FUNZIONI CHE TRATTANO I VARI EVENTI DI TRACE *****
                  node->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("OutInterests", MakeBoundCallback(&InterestTrace, streamInterest));
                  node->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InInterests", MakeBoundCallback(&InterestTrace, streamInterest));
                  node->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("AggregateInterests", MakeBoundCallback(&InterestTrace, streamInterest));
                  node->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("OutData", MakeBoundCallback(&DataTrace, streamData));
                  node->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InData", MakeBoundCallback(&DataTrace, streamData));
                  z = z+1;
          }
  }
  else
  {
          z = 0;
          for (NodeContainer::Iterator node = coreNodes.Begin(); node != coreNodes.End(); ++node)
          {

                  // ******* INTEREST
                  ss << "RESULTS/" <<  simulationType << "/INTEREST/InterestCORE_" << z << stringScenario;
                  fnInterestCore = ss.str();
                  const char *filename_interestCore = fnInterestCore.c_str();
                  ss.str("");

                  // ******* DATA
                  ss << "RESULTS/" <<  simulationType << "/DATA/DataCORE_" << z << stringScenario;
                  fnDataCore = ss.str();
                  const char *filename_dataCore = fnDataCore.c_str();
                  ss.str("");

                  // ******** STREAM DI OUTPUT ***************
                  Ptr<OutputStreamWrapper> streamInterest = asciiTraceHelper.CreateFileStream(filename_interestCore);
                  Ptr<OutputStreamWrapper> streamData = asciiTraceHelper.CreateFileStream(filename_dataCore);

                   // ***** ASSOCIAZIONE ALLE FUNZIONI CHE TRATTANO I VARI EVENTI DI TRACE *****
                  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("OutInterests", MakeBoundCallback(&InterestTrace, streamInterest));
                  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InInterests", MakeBoundCallback(&InterestTrace, streamInterest));
                  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("AggregateInterests", MakeBoundCallback(&InterestTrace, streamInterest));
                  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("OutData", MakeBoundCallback(&DataTrace, streamData));
                  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InData", MakeBoundCallback(&DataTrace, streamData));
                  z = z+1;
          }
  }



  // *****  App-Level Tracing -
  z=0;
  for (NodeContainer::Iterator node_app = clientNodes.Begin(); node_app != clientNodes.End(); ++node_app)
  {
          // ******** DATA APP
          ss << "RESULTS/" <<  simulationType << "/DATA/APP/DataAPP_" << z << stringScenario;
          fnDataAppClient = ss.str();
          const char *filename_data_appClient = fnDataAppClient.c_str();
          ss.str("");

          // ******** INTEREST APP
          ss << "RESULTS/" <<  simulationType << "/INTEREST/APP/InterestAPP_" << z << stringScenario;
          fnInterestAppClient = ss.str();
          const char *filename_interest_appClient = fnInterestAppClient.c_str();
          ss.str("");

          // ******** DOWNLOAD
          ss << "RESULTS/" <<  simulationType << "/DOWNLOAD/APP/Download_" << z << stringScenario;
          fnDwnTime = ss.str();
          const char *filename_download_time = fnDwnTime.c_str();
          ss.str("");

          // ******** STREAM DI OUTPUT APP ***************
      Ptr<OutputStreamWrapper> streamDataApp = asciiTraceHelper.CreateFileStream(filename_data_appClient);
          Ptr<OutputStreamWrapper> streamInterestApp = asciiTraceHelper.CreateFileStream(filename_interest_appClient);
      Ptr<OutputStreamWrapper> streamDownloadTime = asciiTraceHelper.CreateFileStream(filename_download_time);

      // ***** ASSOCIAZIONE ALLE FUNZIONI CHE TRATTANO I VARI EVENTI DI TRACE *****
      (*node_app)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("DataInCacheApp", MakeBoundCallback(&DataAppTrace, streamDataApp));
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("InterestApp",MakeBoundCallback(&InterestAppTrace, streamInterestApp));
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("TimeOutTrace",MakeBoundCallback(&InterestAppTrace, streamInterestApp));
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("NumMaxRtx",MakeBoundCallback(&InterestAppTrace, streamInterestApp));
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("UncompleteFile",MakeBoundCallback(&InterestAppTrace, streamInterestApp));
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("DownloadTime",MakeBoundCallback(&DownloadTimeTrace, streamDownloadTime));
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("DownloadTimeFile",MakeBoundCallback(&DownloadTimeTrace, streamDownloadTime));
      z=z+1;
  }

  // **************************************************************************************************************************

  Simulator::Stop (finishTime);

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();

  //   ** [MT] ** Print the cache of each node at the end of the simulation.
  for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node ++)
  {
     std::cout << "Node #" << ((*node)->GetId ()+1) << std::endl;
     (*node)->GetObject<ndn::ContentStore> ()->Print (std::cout);
     std::cout << std::endl;
  }


  Simulator::Destroy ();
  NS_LOG_INFO ("Done!");

  return 0;
}


// *************************

void InterestTrace(Ptr<OutputStreamWrapper> stream, Ptr<const Interest> header, Ptr<const Face> face, std::string eventType, std::string nodeType)
{
        *stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  eventType << "\t" << face->GetId() <<  "\t--"  << std::endl;
}

void DataTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, Ptr<const Face> face, std::string eventType, std::string nodeType)
{
        *stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  eventType << "\t" << face->GetId() << "\t--"  << std::endl;
}

void DataAppTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, std::string eventType, std::string nodeType)
{
        *stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  eventType << "\t--"  << std::endl;
}

void InterestAppTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, std::string eventType, std::string nodeType)
{
        *stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" << time_sent << "\t" <<  eventType << "\t--" << std::endl;
}

void DownloadTimeTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, int64_t downloadTime, uint32_t dist, std::string eventType, std::string nodeType)
{
        *stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" << time_sent << "\t" <<  eventType << "\t" << "--" << "\t" << downloadTime << "\t" << dist << std::endl;
}



// ******* READ ADJ MATRIX
std::vector<std::vector<bool> > readAdjMat (std::string adj_mat_file_name)
{
  std::ifstream adj_mat_file;
  adj_mat_file.open (adj_mat_file_name.c_str (), std::ios::in);
  if (adj_mat_file.fail ())
    {
      NS_FATAL_ERROR ("File " << adj_mat_file_name.c_str () << " not found");
    }
  std::vector<std::vector<bool> > array;
  int i = 0;
  int n_nodes = 0;

  while (!adj_mat_file.eof ())
    {
      std::string line;
      getline (adj_mat_file, line);
      if (line == "")
        {
          NS_LOG_WARN ("WARNING: Ignoring blank row in the array: " << i);
          break;
        }

      std::istringstream iss (line);
      bool element;
      std::vector<bool> row;
      int j = 0;

      while (iss >> element)
        {
          row.push_back (element);
          j++;
        }

      if (i == 0)
        {
          n_nodes = j;
        }

      if (j != n_nodes )
        {
          NS_LOG_ERROR ("ERROR: Number of elements in line " << i << ": " << j << " not equal to number of elements in line 0: " << n_nodes);
          NS_FATAL_ERROR ("ERROR: The number of rows is not equal to the number of columns! in the adjacency matrix");
        }
      else
        {
          array.push_back (row);
        }
      i++;
    }

  if (i != n_nodes)
    {
      NS_LOG_ERROR ("There are " << i << " rows and " << n_nodes << " columns.");
      NS_FATAL_ERROR ("ERROR: The number of rows is not equal to the number of columns! in the adjacency matrix");
    }

  adj_mat_file.close ();
  return array;

}

// ******* REA COORDINATE FILE
std::vector<std::vector<double> > readCordinatesFile (std::string node_coordinates_file_name)
{
  std::ifstream node_coordinates_file;
  node_coordinates_file.open (node_coordinates_file_name.c_str (), std::ios::in);
  if (node_coordinates_file.fail ())
    {
      NS_FATAL_ERROR ("File " << node_coordinates_file_name.c_str () << " not found");
    }
  std::vector<std::vector<double> > coord_array;
  int m = 0;

  while (!node_coordinates_file.eof ())
    {
      std::string line;
      getline (node_coordinates_file, line);

      if (line == "")
        {
          NS_LOG_WARN ("WARNING: Ignoring blank row: " << m);
          break;
        }

      std::istringstream iss (line);
      double coordinate;
      std::vector<double> row;
      int n = 0;
      while (iss >> coordinate)
        {
          row.push_back (coordinate);
          n++;
        }

      if (n != 2)
        {
          NS_LOG_ERROR ("ERROR: Number of elements at line#" << m << " is "  << n << " which is not equal to 2 for node coordinates file");
          exit (1);
        }

      else
        {
          coord_array.push_back (row);
        }
      m++;
    }
  node_coordinates_file.close ();
  return coord_array;

}

