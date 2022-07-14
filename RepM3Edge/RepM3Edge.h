#pragma once

#include "ShapeProperties.h"
#include "IIqrfAdapt.h"
#include "IWorkerThread.h"
#include "IMsgService.h"
#include "IIdentityProvider.h"
#include "ILaunchService.h"
#include "IConfigurationService.h"
#include "ICommandService.h"
#include "ITraceService.h"

namespace lgmc {
  class RepM3Edge
  {
  public:
    RepM3Edge();
    virtual ~RepM3Edge();

    void activate(const shape::Properties *props = 0);
    void deactivate();
    void modify(const shape::Properties *props);

    void attachInterface(oegw::IIqrfAdapt* iface);
    void detachInterface(oegw::IIqrfAdapt* iface);

    void attachInterface(oegw::IWorkerThread* iface);
    void detachInterface(oegw::IWorkerThread* iface);

    void attachInterface(oegw::IIdentityProvider* iface);
    void detachInterface(oegw::IIdentityProvider* iface);

    void attachInterface(oegw::IMsgService* iface);
    void detachInterface(oegw::IMsgService* iface);

    void attachInterface(shape::IConfigurationService* iface);
    void detachInterface(shape::IConfigurationService* iface);

    void attachInterface(shape::ILaunchService* iface);
    void detachInterface(shape::ILaunchService* iface);

    void attachInterface(shape::ICommandService* iface);
    void detachInterface(shape::ICommandService* iface);

    void attachInterface(shape::ITraceService* iface);
    void detachInterface(shape::ITraceService* iface);

  private:
    class Imp;
    Imp* m_imp;
  };
}
