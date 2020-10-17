#ifndef __INET_CARNETWORK_H
#define __INET_CARNETWORK_H

#include "inet/common/INETDefs.h"

#include "inet/mobility/static/StationaryMobility.h"

namespace inet {

/**
 * @brief Mobility model which places all hosts at constant distances
 *  within the simulation area (resulting in a regular grid).
 *
 * @ingroup mobility
 * @author Isabel Dietrich
 */

class INET_API carNetwork : public StationaryMobility
{
  protected:
    /** @brief Initializes the position according to the mobility model. */
    virtual void setInitialPosition() override;

  public:
    carNetwork() {};
};

} // namespace inet

#endif // ifndef __INET_STATICGRIDMOBILITY_H
