#ifndef __PREFETCH_BUFFER_HH__
#define __PREFETCH_BUFFER_HH__

#include "CHIPort/P2cFifo.hh"
#include "Configure/Configure.hh"
namespace dmu {
namespace Port {

class PrefetchBuffer {

public:
  explicit PrefetchBuffer(const Configure &configure);

private:
  const Configure &m_configure;
};

} // namespace Port
} // namespace dmu

#endif