#include "carNetwork.h"

namespace inet {

Define_Module(carNetwork);

void carNetwork::setInitialPosition()
{

    double marginX = par("marginX");
    double marginY = par("marginY");
    double separationX = par("separationX");
    double separationY = par("separationY");
    int columns = par("columns");
    int rows = par("rows");
    double nodeprob = par("nodeprob");
    int matrixSize = rows * columns;
    int numHosts = matrixSize * nodeprob;

 //   if (numHosts > rows * columns)
  //      throw cRuntimeError("parameter error: numHosts > rows * columns");

    int index = subjectModule->getIndex();

    int modifiedIndex = index * 2;

    if (modifiedIndex >= numHosts && modifiedIndex < matrixSize )
    {
        index = modifiedIndex;
    }

    int row = index / columns;
    int col = index % columns;
    lastPosition.x = constraintAreaMin.x + marginX + (col + 0.5) * separationX;
    lastPosition.y = constraintAreaMin.y + marginY + (row + 0.5) * separationY;
    lastPosition.z = par("initialZ");
    recordScalar("x", lastPosition.x);
    recordScalar("y", lastPosition.y);
    recordScalar("z", lastPosition.z);



}

}
