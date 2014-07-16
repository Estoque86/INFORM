/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
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
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#ifndef _NDN_QTAB_IMPL_H_
#define	_NDN_QTAB_IMPL_H_

#include "ndn-qtab.h"

#include "ns3/log.h"
#include "ns3/simulator.h"

#include "../../utils/trie/trie-with-policy.h"
#include "ndn-qtab-entry-impl.h"

#include "ns3/ndn-interest.h"
#include "ns3/ndn-content-object.h"
#include "ns3/ndn-forwarding-strategy.h"
#include "ns3/ndn-name.h"


namespace ns3 {
namespace ndn {

class ForwardingStrategy;

namespace qtab {

/**
 * \ingroup ndn
 * \brief Class implementing Pending Interests Table
 */
template<class Policy>
class QtabImpl : public Qtab
              , protected ndnSIM::trie_with_policy<Name,
                                                   ndnSIM::smart_pointer_payload_traits< EntryImpl< QtabImpl< Policy > > >,
                                                   // ndnSIM::persistent_policy_traits
                                                   Policy
                                                   >
{
public:
  typedef ndnSIM::trie_with_policy<Name,
                                   ndnSIM::smart_pointer_payload_traits< EntryImpl< QtabImpl< Policy > > >,
                                   // ndnSIM::persistent_policy_traits
                                   Policy
                                   > super;
  typedef EntryImpl< QtabImpl< Policy > > entry;

  /**
   * \brief Interface ID
   *
   * \return interface ID
   */
  static TypeId GetTypeId ();

  /**
   * \brief PIT constructor
   */
  QtabImpl ();

  /**
   * \brief Destructor
   */
  virtual ~QtabImpl ();

  // inherited from Pit
  virtual Ptr<Entry>
  LookupQtab (const ContentObject &header);

  virtual Ptr<Entry>
  LookupQtab (const Interest &header);

  virtual Ptr<Entry>
  FindQtab (const Name &prefix);

  virtual Ptr<Entry>
  CreateQtab (Ptr<const Interest> header, Ptr<fib::Entry> fibEntry, uint32_t interfaces);

  virtual void
  MarkErasedQtab (Ptr<Entry> entry);

  virtual void
  PrintQtab (std::ostream &os) const;

  virtual uint32_t
  GetSizeQtab () const;

  virtual Ptr<Entry>
  BeginQtab ();

  virtual Ptr<Entry>
  EndQtab ();

  virtual Ptr<Entry>
  NextQtab (Ptr<Entry>);

  const typename super::policy_container &
  GetPolicy () const { return super::getPolicy (); }

  typename super::policy_container &
  GetPolicy () { return super::getPolicy (); }

protected:
  void RescheduleCleaning ();
  void CleanExpired ();

  // inherited from Object class
  virtual void NotifyNewAggregate (); ///< @brief Even when object is aggregated to another Object
  virtual void DoDispose (); ///< @brief Do cleanup

private:
  uint32_t
  GetMaxSize () const;

  void
  SetMaxSize (uint32_t maxSize);

  uint32_t
  GetCurrentSize () const;

private:
  EventId m_cleanEvent;
  Ptr<Fib> m_fib; ///< \brief Link to FIB table
  Ptr<ForwardingStrategy> m_forwardingStrategy;

  static LogComponent g_log; ///< @brief Logging variable

  // indexes
  typedef
  boost::intrusive::multiset<entry,
                        boost::intrusive::compare < TimestampIndex< entry > >,
                        boost::intrusive::member_hook< entry,
                                                       boost::intrusive::set_member_hook<>,
                                                       &entry::time_hook_>
                        > time_index;
  time_index i_time;

  friend class EntryImpl< QtabImpl >;
};

//////////////////////////////////////////
////////// Implementation ////////////////
//////////////////////////////////////////

template<class Policy>
LogComponent QtabImpl< Policy >::g_log = LogComponent (("ndn.qtab." + Policy::GetName ()).c_str ());


template<class Policy>
TypeId
QtabImpl< Policy >::GetTypeId ()
{
  static TypeId tid = TypeId (("ns3::ndn::qtab::"+Policy::GetName ()).c_str ())
    .SetGroupName ("Ndn")
    .SetParent<Qtab> ()
    .AddConstructor< QtabImpl< Policy > > ()
    .AddAttribute ("MaxSize",
                   "Set maximum size of QTAB in bytes. If 0, limit is not enforced",
                   UintegerValue (0),
                   MakeUintegerAccessor (&QtabImpl< Policy >::GetMaxSize,
                                         &QtabImpl< Policy >::SetMaxSize),
                   MakeUintegerChecker<uint32_t> ())

    .AddAttribute ("CurrentSize", "Get current size of QTAB in bytes",
                   TypeId::ATTR_GET,
                   UintegerValue (0),
                   MakeUintegerAccessor (&QtabImpl< Policy >::GetCurrentSize),
                   MakeUintegerChecker<uint32_t> ())
    ;

  return tid;
}

template<class Policy>
uint32_t
QtabImpl<Policy>::GetCurrentSize () const
{
  return super::getPolicy ().size ();
}

template<class Policy>
QtabImpl<Policy>::QtabImpl ()
{
}

template<class Policy>
QtabImpl<Policy>::~QtabImpl ()
{
}

template<class Policy>
uint32_t
QtabImpl<Policy>::GetMaxSize () const
{
  return super::getPolicy ().get_max_size ();
}

template<class Policy>
void
QtabImpl<Policy>::SetMaxSize (uint32_t maxSize)
{
  super::getPolicy ().set_max_size (maxSize);
}

template<class Policy>
void
QtabImpl<Policy>::NotifyNewAggregate ()
{
  if (m_fib == 0)
    {
      m_fib = GetObject<Fib> ();
    }
  if (m_forwardingStrategy == 0)
    {
      m_forwardingStrategy = GetObject<ForwardingStrategy> ();
    }

  Qtab::NotifyNewAggregate ();
}

template<class Policy>
void
QtabImpl<Policy>::DoDispose ()
{
  super::clear ();

  m_forwardingStrategy = 0;
  m_fib = 0;

  Qtab::DoDispose ();
}

template<class Policy>
void
QtabImpl<Policy>::RescheduleCleaning ()
{
  // m_cleanEvent.Cancel ();
  Simulator::Remove (m_cleanEvent); // slower, but better for memory
  if (i_time.empty ())
    {
      // NS_LOG_DEBUG ("No items in PIT");
      return;
    }

  Time nextEvent = i_time.begin ()->GetExpireTime () - Simulator::Now ();
  if (nextEvent <= 0) nextEvent = Seconds (0);

  NS_LOG_DEBUG ("Schedule next cleaning in " <<
                nextEvent.ToDouble (Time::S) << "s (at " <<
                i_time.begin ()->GetExpireTime () << "s abs time");

  m_cleanEvent = Simulator::Schedule (nextEvent,
                                      &QtabImpl<Policy>::CleanExpired, this);
}

template<class Policy>
void
QtabImpl<Policy>::CleanExpired ()
{
  NS_LOG_LOGIC ("Cleaning QTAB. Total: " << i_time.size ());
  Time now = Simulator::Now ();

  // uint32_t count = 0;
  while (!i_time.empty ())
    {
      typename time_index::iterator entry = i_time.begin ();
      if (entry->GetExpireTime () <= now) // is the record stale?
        {
          //m_forwardingStrategy->WillEraseTimedOutPendingInterest (entry->to_iterator ()->payload ());  // ** [MT] ** Tracing disabled
          super::erase (entry->to_iterator ());
          uint32_t thisNode = m_fib->GetObject<Node>()->GetId();
          NS_LOG_UNCOND("QtabImpl/CE - NODE:\t" << thisNode << "\t Erasing qTabEntry:\t" << entry->to_iterator()->payload());
          // count ++;
        }
      else
        break; // nothing else to do. All later records will not be stale
    }

  if (super::getPolicy ().size ())
    {
      NS_LOG_DEBUG ("Size: " << super::getPolicy ().size ());
      NS_LOG_DEBUG ("i_time size: " << i_time.size ());
    }
  RescheduleCleaning ();
}

// ** [MT] ** ORIGINAL VERSION. It is a LongestPrefixMatch searching
/*template<class Policy>
Ptr<Entry>
PitImpl<Policy>::Lookup (const ContentObject &header)
{
  /// @todo use predicate to search with exclude filters
  typename super::iterator item = super::longest_prefix_match_if (header.GetName (), EntryIsNotEmpty ());

  if (item == super::end ())
    return 0;
  else
    return item->payload (); // which could also be 0
}*/

// ** [MT] ** MODIFIED VERSION. It is an ExactMatch searching
template<class Policy>
Ptr<Entry>
QtabImpl<Policy>::LookupQtab (const ContentObject &header)
{
  typename super::iterator foundItem, lastItem;
  bool reachLast;
  boost::tie (foundItem, reachLast, lastItem) = super::getTrie ().find (header.GetName ());

  if (!reachLast || lastItem == super::end ())
   return 0;
  else
    return lastItem->payload (); // which could also be 0
}

// ** [MT] ** When receiving an Interest, is always an exact match searching
template<class Policy>
Ptr<Entry>
QtabImpl<Policy>::LookupQtab (const Interest &header)
{
  // NS_LOG_FUNCTION (header.GetName ());
  NS_ASSERT_MSG (m_fib != 0, "FIB should be set");
  NS_ASSERT_MSG (m_forwardingStrategy != 0, "Forwarding strategy  should be set");

  typename super::iterator foundItem, lastItem;
  bool reachLast;
  boost::tie (foundItem, reachLast, lastItem) = super::getTrie ().find (header.GetName ());

  if (!reachLast || lastItem == super::end ())
    return 0;
  else
    return lastItem->payload (); // which could also be 0
}

template<class Policy>
Ptr<Entry>
QtabImpl<Policy>::FindQtab (const Name &prefix)
{
  typename super::iterator item = super::find_exact (prefix);

  if (item == super::end ())
    return 0;
  else
    return item->payload ();
}


template<class Policy>
Ptr<Entry>
QtabImpl<Policy>::CreateQtab (Ptr<const Interest> header, Ptr<fib::Entry> fibEntry, uint32_t interfaces)
{
  NS_LOG_DEBUG (header->GetName ());

  Ptr< entry > newEntry = ns3::Create< entry > (boost::ref (*this), header, fibEntry, interfaces);
  std::pair< typename super::iterator, bool > result = super::insert (header->GetName (), newEntry);
  if (result.first != super::end ())
    {
      if (result.second)
        {
          newEntry->SetTrie (result.first);
          return newEntry;
        }
      else
        {
          // should we do anything?
          // update payload? add new payload?
          return result.first->payload ();
        }
    }
  else
    return 0;
}



template<class Policy>
void
QtabImpl<Policy>::MarkErasedQtab (Ptr<Entry> item)
{
//  if (this->m_QtabEntryPruningTimout.IsZero ())
//    {
      super::erase (StaticCast< entry > (item)->to_iterator ());
//    }
//  else
//    {
//      item->OffsetLifetime (this->m_QtabEntryPruningTimout - item->GetExpireTime () + Simulator::Now ());
//    }
}


template<class Policy>
void
QtabImpl<Policy>::PrintQtab (std::ostream& os) const
{
  // !!! unordered_set imposes "random" order of item in the same level !!!
  typename super::parent_trie::const_recursive_iterator item (super::getTrie ()), end (0);
  for (; item != end; item++)
    {
      if (item->payload () == 0) continue;

      os << item->payload ()->GetPrefix () << "\t" << *item->payload () << "\n";
    }
}

template<class Policy>
uint32_t
QtabImpl<Policy>::GetSizeQtab () const
{
  return super::getPolicy ().size ();
}

template<class Policy>
Ptr<Entry>
QtabImpl<Policy>::BeginQtab ()
{
  typename super::parent_trie::recursive_iterator item (super::getTrie ()), end (0);
  for (; item != end; item++)
    {
      if (item->payload () == 0) continue;
      break;
    }

  if (item == end)
    return EndQtab ();
  else
    return item->payload ();
}

template<class Policy>
Ptr<Entry>
QtabImpl<Policy>::EndQtab ()
{
  return 0;
}

template<class Policy>
Ptr<Entry>
QtabImpl<Policy>::NextQtab (Ptr<Entry> from)
{
  if (from == 0) return 0;

  typename super::parent_trie::recursive_iterator
    item (*StaticCast< entry > (from)->to_iterator ()),
    end (0);

  for (item++; item != end; item++)
    {
      if (item->payload () == 0) continue;
      break;
    }

  if (item == end)
    return EndQtab ();
  else
    return item->payload ();
}


} // namespace qtab
} // namespace ndn
} // namespace ns3

#endif	/* NDN_QTAB_IMPL_H */

