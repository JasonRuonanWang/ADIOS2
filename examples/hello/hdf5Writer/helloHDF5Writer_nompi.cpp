/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 *
 * helloHDF5Writer_nompi.cpp  no mpi version of helloHDF5Writer.cpp
 *
 *  Created on: March 20, 2017
 *      Author: Junmin
 */

#include <ios>       //std::ios_base::failure
#include <iostream>  //std::cout
#include <stdexcept> //std::invalid_argument std::exception
#include <vector>

#include <adios2.h>

int main(int argc, char *argv[])
{
    /** Application variable */
    std::vector<float> myFloats = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    const std::size_t Nx = myFloats.size();

    try
    {
        /** ADIOS class factory of IO class objects, DebugON is recommended */
        adios::ADIOS adios(adios::DebugON);

        /*** IO class object: settings and factory of Settings: Variables,
         * Parameters, Transports, and Execution: Engines */
        adios::IO &hdf5IO = adios.DeclareIO("HDFFileIO");
        hdf5IO.SetEngine("HDF5Writer");
        hdf5IO.AddTransport("file");

        /** global array : name, { shape (total) }, { start (local) }, { count
         * (local) }, all are constant dimensions */
        adios::Variable<float> &bpFloats = hdf5IO.DefineVariable<float>(
            "bpFloats", {}, {}, {Nx}, adios::ConstantDims);

        /** Engine derived class, spawned to start IO operations */
        auto hdf5Writer = hdf5IO.Open("myVector.bp", adios::OpenMode::Write);

        if (!hdf5Writer)
        {
            throw std::ios_base::failure(
                "ERROR: hdf5Writer not created at Open\n");
        }

        /** Write variable for buffering */
        hdf5Writer->Write<float>(bpFloats, myFloats.data());

        /** Create bp file, engine becomes unreachable after this*/
        hdf5Writer->Close();
    }
    catch (std::invalid_argument &e)
    {
        std::cout << "Invalid argument exception, STOPPING PROGRAM\n";
        std::cout << e.what() << "\n";
    }
    catch (std::ios_base::failure &e)
    {
        std::cout << "IO System base failure exception, STOPPING PROGRAM\n";
        std::cout << e.what() << "\n";
    }
    catch (std::exception &e)
    {
        std::cout << "Exception, STOPPING PROGRAM from rank\n";
        std::cout << e.what() << "\n";
    }

    return 0;
}