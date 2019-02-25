#include "otsdaq-demo/ArtModules/detail/RawEventQueueReader.hh"
#include "otsdaq-demo/Overlays/FragmentType.hh"

#include <messagefacility/MessageLogger/MessageLogger.h>

ots::detail::RawEventQueueReader::RawEventQueueReader(
    fhicl::ParameterSet const &ps, art::ProductRegistryHelper &help,
    art::SourceHelper const &pm)
    : artdaq::detail::RawEventQueueReader(ps, help, pm) {
  TLOG(TLVL_INFO, "RawEventQueueReader") << "OtsInput Constructor!";
  for (auto &name : names) {
    TLOG(TLVL_INFO, "RawEventQueueReader")
        << "Adding fragment type " << name
        << " to fragment_type_map, and registering with the "
           "ProductRegistryHelper";
    fragment_type_map_[toFragmentType(name)] = name;
    help.reconstitutes<artdaq::Fragments, art::InEvent>(pretend_module_name,
                                                        name);
  }
}
