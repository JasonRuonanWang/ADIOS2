/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 */
#include <cstdint>
#include <cstdlib> // std::rand
#include <cstring>

#include <iostream>
#include <numeric> //std::iota
#include <stdexcept>

#include <adios2.h>

#include <gtest/gtest.h>

std::string engineName; // comes from command line

template <class T>
static T Random100()
{
    return std::rand() % 100;
}

void PNGAccuracy2D(const int compressionLevel)
{
    // Each process would write a 1x8 array and all processes would
    // form a mpiSize * Nx 1D array
    const std::string fname("BPWRPNG2D_" + std::to_string(compressionLevel) +
                            ".bp");

    int mpiRank = 0, mpiSize = 1;
    // Number of rows
    const size_t height = 2;
    const size_t width = 80;

    // Number of steps
    const size_t NSteps = 1;

    std::vector<int8_t> i8s(height * width);
    std::vector<int16_t> i16s(height * width);
    std::vector<int32_t> i32s(height * width);
    std::vector<uint8_t> u8s(height * width);
    std::vector<uint16_t> u16s(height * width);
    std::vector<uint32_t> u32s(height * width);
    std::vector<float> r32s(height * width);

    // range 0 to 100*50
    for (auto i = 0; i < height * width; ++i)
    {
        i8s[i] = Random100<int8_t>();
        i16s[i] = Random100<int16_t>();
        i32s[i] = i;
        u8s[i] = Random100<uint8_t>();
        u16s[i] = Random100<uint16_t>();
        u32s[i] = i;
        r32s[i] = i;
    }

#ifdef ADIOS2_HAVE_MPI
    MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpiSize);
#endif

#ifdef ADIOS2_HAVE_MPI
    adios2::ADIOS adios(MPI_COMM_WORLD, adios2::DebugON);
#else
    adios2::ADIOS adios(true);
#endif
    {
        adios2::IO io = adios.DeclareIO("TestIO");

        if (!engineName.empty())
        {
            io.SetEngine(engineName);
        }
        else
        {
            // Create the BP Engine
            io.SetEngine("BPFile");
        }

        const adios2::Dims shape{static_cast<size_t>(height * mpiSize), width};
        const adios2::Dims start{static_cast<size_t>(height * mpiRank), 0};
        const adios2::Dims count{height, width};

        auto var_i8 = io.DefineVariable<int8_t>("i8", shape, start, count,
                                                adios2::ConstantDims);
        auto var_i16 = io.DefineVariable<int16_t>("i16", shape, start, count,
                                                  adios2::ConstantDims);
        auto var_i32 = io.DefineVariable<int32_t>("i32", shape, start, count,
                                                  adios2::ConstantDims);

        auto var_u8 = io.DefineVariable<uint8_t>("u8", shape, start, count,
                                                 adios2::ConstantDims);
        auto var_u16 = io.DefineVariable<uint16_t>("u16", shape, start, count,
                                                   adios2::ConstantDims);
        auto var_u32 = io.DefineVariable<uint32_t>("u32", shape, start, count,
                                                   adios2::ConstantDims);

        auto var_r32 = io.DefineVariable<float>("r32", shape, start, count,
                                                adios2::ConstantDims);

        // add operations
        adios2::Operator PNGOp = adios.DefineOperator("PNGCompressor", "PNG");

        //        var_i8.AddOperation(
        //            PNGOp, {{"compression_level",
        //            std::to_string(compressionLevel)}});
        //        var_i16.AddOperation(
        //            PNGOp, {{"compression_level",
        //            std::to_string(compressionLevel)}});
        var_i32.AddOperation(
            PNGOp, {{"compression_level", std::to_string(compressionLevel)}});

        //        var_u8.AddOperation(
        //            PNGOp, {{"compression_level",
        //            std::to_string(compressionLevel)}});
        //        var_u16.AddOperation(
        //            PNGOp, {{"compression_level",
        //            std::to_string(compressionLevel)}});
        var_u32.AddOperation(
            PNGOp, {{"compression_level", std::to_string(compressionLevel)}});

        var_r32.AddOperation(
            PNGOp, {{"compression_level", std::to_string(compressionLevel)}});

        adios2::Engine bpWriter = io.Open(fname, adios2::Mode::Write);

        for (auto step = 0; step < NSteps; ++step)
        {
            bpWriter.BeginStep();
            bpWriter.Put<int8_t>("i8", i8s.data());
            bpWriter.Put<int16_t>("i16", i16s.data());
            bpWriter.Put<int32_t>("i32", i32s.data());

            bpWriter.Put<uint8_t>("u8", u8s.data());
            bpWriter.Put<uint16_t>("u16", u16s.data());
            bpWriter.Put<uint32_t>("u32", u32s.data());

            bpWriter.Put<float>("r32", r32s.data());

            bpWriter.EndStep();
        }

        bpWriter.Close();
    }

    {
        adios2::IO io = adios.DeclareIO("ReadIO");

        if (!engineName.empty())
        {
            io.SetEngine(engineName);
        }
        else
        {
            // Create the BP Engine
            io.SetEngine("BPFile");
        }

        adios2::Engine bpReader = io.Open(fname, adios2::Mode::Read);

        unsigned int t = 0;

        const adios2::Dims start{mpiRank * height, 0};
        const adios2::Dims count{height, width};
        const adios2::Box<adios2::Dims> sel(start, count);

        std::vector<int8_t> decompressedi8s(height * width);
        std::vector<int16_t> decompressedi16s(height * width);
        std::vector<int32_t> decompressedi32s(height * width);
        std::vector<uint8_t> decompressedu8s(height * width);
        std::vector<uint16_t> decompressedu16s(height * width);
        std::vector<uint32_t> decompressedu32s(height * width);
        std::vector<float> decompressedr32s(height * width);

        while (bpReader.BeginStep() == adios2::StepStatus::OK)
        {
            auto var_i8 = io.InquireVariable<int8_t>("i8");
            EXPECT_TRUE(var_i8);
            ASSERT_EQ(var_i8.ShapeID(), adios2::ShapeID::GlobalArray);
            ASSERT_EQ(var_i8.Steps(), NSteps);
            ASSERT_EQ(var_i8.Shape()[0], mpiSize * height);
            ASSERT_EQ(var_i8.Shape()[1], width);

            auto var_i16 = io.InquireVariable<int16_t>("i16");
            EXPECT_TRUE(var_i16);
            ASSERT_EQ(var_i16.ShapeID(), adios2::ShapeID::GlobalArray);
            ASSERT_EQ(var_i16.Steps(), NSteps);
            ASSERT_EQ(var_i16.Shape()[0], mpiSize * height);
            ASSERT_EQ(var_i16.Shape()[1], width);

            auto var_i32 = io.InquireVariable<int32_t>("i32");
            EXPECT_TRUE(var_i32);
            ASSERT_EQ(var_i32.ShapeID(), adios2::ShapeID::GlobalArray);
            ASSERT_EQ(var_i32.Steps(), NSteps);
            ASSERT_EQ(var_i32.Shape()[0], mpiSize * height);
            ASSERT_EQ(var_i32.Shape()[1], width);

            auto var_u8 = io.InquireVariable<uint8_t>("u8");
            EXPECT_TRUE(var_u8);
            ASSERT_EQ(var_u8.ShapeID(), adios2::ShapeID::GlobalArray);
            ASSERT_EQ(var_u8.Steps(), NSteps);
            ASSERT_EQ(var_u8.Shape()[0], mpiSize * height);
            ASSERT_EQ(var_u8.Shape()[1], width);

            auto var_u16 = io.InquireVariable<uint16_t>("u16");
            EXPECT_TRUE(var_u16);
            ASSERT_EQ(var_u16.ShapeID(), adios2::ShapeID::GlobalArray);
            ASSERT_EQ(var_u16.Steps(), NSteps);
            ASSERT_EQ(var_u16.Shape()[0], mpiSize * height);
            ASSERT_EQ(var_u16.Shape()[1], width);

            auto var_u32 = io.InquireVariable<uint32_t>("u32");
            EXPECT_TRUE(var_u32);
            ASSERT_EQ(var_u32.ShapeID(), adios2::ShapeID::GlobalArray);
            ASSERT_EQ(var_u32.Steps(), NSteps);
            ASSERT_EQ(var_u32.Shape()[0], mpiSize * height);
            ASSERT_EQ(var_u32.Shape()[1], width);

            auto var_r32 = io.InquireVariable<float>("r32");
            EXPECT_TRUE(var_r32);
            ASSERT_EQ(var_r32.ShapeID(), adios2::ShapeID::GlobalArray);
            ASSERT_EQ(var_r32.Steps(), NSteps);
            ASSERT_EQ(var_r32.Shape()[0], mpiSize * height);
            ASSERT_EQ(var_r32.Shape()[1], width);

            var_i8.SetSelection(sel);
            var_i16.SetSelection(sel);
            var_i32.SetSelection(sel);
            var_u8.SetSelection(sel);
            var_u16.SetSelection(sel);
            var_u32.SetSelection(sel);

            var_r32.SetSelection(sel);

            bpReader.Get(var_i8, decompressedi8s);
            bpReader.Get(var_i16, decompressedi16s);
            bpReader.Get(var_i32, decompressedi32s);
            bpReader.Get(var_u8, decompressedu8s);
            bpReader.Get(var_u16, decompressedu16s);
            bpReader.Get(var_u32, decompressedu32s);

            bpReader.Get(var_r32, decompressedr32s);

            bpReader.EndStep();

            for (size_t i = 0; i < height * width; ++i)
            {
                std::stringstream ss;
                ss << "t=" << t << " i=" << i << " rank=" << mpiRank;
                std::string msg = ss.str();

                //                EXPECT_EQ(decompressedi8s[i], i8s[i]) << msg;
                //                EXPECT_EQ(decompressedi16s[i], i16s[i]) <<
                //                msg;
                ASSERT_EQ(decompressedi32s[i], i32s[i]) << msg;

                //                EXPECT_EQ(decompressedu8s[i], u8s[i]) << msg;
                //                EXPECT_EQ(decompressedu16s[i], u16s[i]) <<
                //                msg;
                EXPECT_EQ(decompressedu32s[i], u32s[i]) << msg;

                EXPECT_EQ(decompressedr32s[i], r32s[i]) << msg;
            }
            ++t;
        }

        EXPECT_EQ(t, NSteps);

        bpReader.Close();
    }
}

void PNGAccuracy2DSel(const double accuracy)
{
    // Each process would write a 1x8 array and all processes would
    // form a mpiSize * Nx 1D array
    const std::string fname("BPWRPNG2DSel_" + std::to_string(accuracy) + ".bp");

    int mpiRank = 0, mpiSize = 1;
    // Number of rows
    const size_t Nx = 100;
    const size_t Ny = 50;

    // Number of steps
    const size_t NSteps = 1;

    std::vector<float> r32s(Nx * Ny);
    std::vector<double> r64s(Nx * Ny);

    // range 0 to 100*50
    std::iota(r32s.begin(), r32s.end(), 0.f);
    std::iota(r64s.begin(), r64s.end(), 0.);

#ifdef ADIOS2_HAVE_MPI
    MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpiSize);
#endif

#ifdef ADIOS2_HAVE_MPI
    adios2::ADIOS adios(MPI_COMM_WORLD, adios2::DebugON);
#else
    adios2::ADIOS adios(true);
#endif
    {
        adios2::IO io = adios.DeclareIO("TestIO");

        if (!engineName.empty())
        {
            io.SetEngine(engineName);
        }
        else
        {
            // Create the BP Engine
            io.SetEngine("BPFile");
        }

        const adios2::Dims shape{static_cast<size_t>(Nx * mpiSize), Ny};
        const adios2::Dims start{static_cast<size_t>(Nx * mpiRank), 0};
        const adios2::Dims count{Nx, Ny};

        auto var_r32 = io.DefineVariable<float>("r32", shape, start, count,
                                                adios2::ConstantDims);
        auto var_r64 = io.DefineVariable<double>("r64", shape, start, count,
                                                 adios2::ConstantDims);

        // add operations
        adios2::Operator PNGOp = adios.DefineOperator("PNGCompressor", "PNG");

        var_r32.AddOperation(PNGOp,
                             {{"compression_level", std::to_string(accuracy)}});
        var_r64.AddOperation(PNGOp,
                             {{"compression_level", std::to_string(accuracy)}});

        adios2::Engine bpWriter = io.Open(fname, adios2::Mode::Write);

        for (auto step = 0; step < NSteps; ++step)
        {
            bpWriter.BeginStep();
            bpWriter.Put<float>("r32", r32s.data());
            bpWriter.Put<double>("r64", r64s.data());
            bpWriter.EndStep();
        }

        bpWriter.Close();
    }

    {
        adios2::IO io = adios.DeclareIO("ReadIO");

        if (!engineName.empty())
        {
            io.SetEngine(engineName);
        }
        else
        {
            // Create the BP Engine
            io.SetEngine("BPFile");
        }

        adios2::Engine bpReader = io.Open(fname, adios2::Mode::Read);

        auto var_r32 = io.InquireVariable<float>("r32");
        EXPECT_TRUE(var_r32);
        ASSERT_EQ(var_r32.ShapeID(), adios2::ShapeID::GlobalArray);
        ASSERT_EQ(var_r32.Steps(), NSteps);
        ASSERT_EQ(var_r32.Shape()[0], mpiSize * Nx);
        ASSERT_EQ(var_r32.Shape()[1], Ny);

        auto var_r64 = io.InquireVariable<double>("r64");
        EXPECT_TRUE(var_r64);
        ASSERT_EQ(var_r64.ShapeID(), adios2::ShapeID::GlobalArray);
        ASSERT_EQ(var_r64.Steps(), NSteps);
        ASSERT_EQ(var_r64.Shape()[0], mpiSize * Nx);
        ASSERT_EQ(var_r64.Shape()[1], Ny);

        const adios2::Dims start{mpiRank * Nx + Nx / 2, 0};
        const adios2::Dims count{Nx / 2, Ny};
        const adios2::Box<adios2::Dims> sel(start, count);
        var_r32.SetSelection(sel);
        var_r64.SetSelection(sel);

        unsigned int t = 0;
        std::vector<float> decompressedR32s;
        std::vector<double> decompressedR64s;

        while (bpReader.BeginStep() == adios2::StepStatus::OK)
        {
            bpReader.Get(var_r32, decompressedR32s);
            bpReader.Get(var_r64, decompressedR64s);
            bpReader.EndStep();

            for (size_t i = 0; i < Nx / 2 * Ny; ++i)
            {
                std::stringstream ss;
                ss << "t=" << t << " i=" << i << " rank=" << mpiRank;
                std::string msg = ss.str();

                ASSERT_EQ(decompressedR32s[i], r32s[Nx / 2 * Ny + i]) << msg;
                ASSERT_EQ(decompressedR64s[i], r64s[Nx / 2 * Ny + i]) << msg;
            }
            ++t;
        }

        EXPECT_EQ(t, NSteps);

        bpReader.Close();
    }
}

class BPWriteReadPNG : public ::testing::TestWithParam<double>
{
public:
    BPWriteReadPNG() = default;
    virtual void SetUp(){};
    virtual void TearDown(){};
};

TEST_P(BPWriteReadPNG, BPWriteReadPNG2D) { PNGAccuracy2D(GetParam()); }

INSTANTIATE_TEST_CASE_P(PNGAccuracy, BPWriteReadPNG, ::testing::Values(1));

int main(int argc, char **argv)
{
#ifdef ADIOS2_HAVE_MPI
    MPI_Init(nullptr, nullptr);
#endif

    int result;
    ::testing::InitGoogleTest(&argc, argv);

    if (argc > 1)
    {
        engineName = std::string(argv[1]);
    }
    result = RUN_ALL_TESTS();

#ifdef ADIOS2_HAVE_MPI
    MPI_Finalize();
#endif

    return result;
}
