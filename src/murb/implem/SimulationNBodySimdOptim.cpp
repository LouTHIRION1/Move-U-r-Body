#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

#include "SimulationNBodySimdOptim.hpp"

SimulationNBodySimdOptim::SimulationNBodySimdOptim(const unsigned long nBodies, const std::string &scheme, const float soft,
                                           const unsigned long randInit)
    : SimulationNBodyInterface(nBodies, scheme, soft, randInit)
{
    this->flopsPerIte = 26.f * (float)this->getBodies().getN() * (float)this->getBodies().getN();
    this->accelerations.resize(this->getBodies().getN());
}

void SimulationNBodySimdOptim::initIteration()
{
    for (unsigned long iBody = 0; iBody < this->getBodies().getN(); iBody++) {
        this->accelerations[iBody].ax = 0.f;
        this->accelerations[iBody].ay = 0.f;
        this->accelerations[iBody].az = 0.f;
    }
}

void SimulationNBodySimdOptim::computeBodiesAcceleration()
{
    const std::vector<dataAoS_t<float>> &d = this->getBodies().getDataAoS();

    const float softSquared = std::pow(this->soft, 2); // 1 flops
    const float tdt =  3.f / 2.f; // 1 flops
    // flops = n² * 20
    for (unsigned long iBody = 0; iBody < this->getBodies().getN(); iBody++) {
        // flops = n * 26
        for (unsigned long jBody = iBody; jBody < this->getBodies().getN(); jBody++) {
            const float rijx = d[jBody].qx - d[iBody].qx; // 1 flop
            const float rijy = d[jBody].qy - d[iBody].qy; // 1 flop
            const float rijz = d[jBody].qz - d[iBody].qz; // 1 flop

            // compute the || rij ||² distance between body i and body j
            const float rijSquared = std::pow(rijx, 2) + std::pow(rijy, 2) + std::pow(rijz, 2); // 5 flops
            // compute e²
            // compute the acceleration value between body i and body j: || ai || = G.mj / (|| rij ||² + e²)^{3/2}
            const float temp = std::pow(rijSquared + softSquared, tdt); // 2 flops
            const float ai = this->G * d[jBody].m / temp; // 2 flops
            const float aj = this->G * d[iBody].m / temp; // 2 flops


            // add the acceleration value into the acceleration vector: ai += || ai ||.rij
            if (ai != 0) {
                this->accelerations[iBody].ax += ai * rijx; // 2 flops
                this->accelerations[iBody].ay += ai * rijy; // 2 flops
                this->accelerations[iBody].az += ai * rijz; // 2 flops
            }
            if (aj != 0) {
                this->accelerations[jBody].ax += aj * -rijx; // 2 flops
                this->accelerations[jBody].ay += aj * -rijy; // 2 flops
                this->accelerations[jBody].az += aj * -rijz; // 2 flops
            }
        }
    }
}

void SimulationNBodySimdOptim::computeOneIteration()
{
    this->initIteration();
    this->computeBodiesAcceleration();
    // time integration
    this->bodies.updatePositionsAndVelocities(this->accelerations, this->dt);
}
