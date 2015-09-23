#include "gtest/gtest.h"
#include "carlsim_tests.h"

#include <carlsim.h>

#if defined(WIN32) || defined(WIN64)
#include <periodic_spikegen.h>
#endif

/// ****************************************************************************
/// current-based (cuba) model
/// ****************************************************************************


/*!
 * \brief testing CARLsim CUBA outputs vs data
 *
 * This tests to make sure that the firing rates closely resemble a
 * Matlab implementation of an Izhikevich CUBA neuron. The matlab script
 * is found in tests/scripts and is called runCUBA.m. There are two test cases:
 * a test case that produces lower firing (LF) rates in the output RS neuron
 * (1 Hz) and a test case that produces higher firing rates (HF) in the output
 * RS neuron (13 Hz). The parameters used as input to the Matlab script and
 * this test script are as follows:
 *
 * LF case: input firing rate: 50 Hz
 *          weight value from input to output neuron: 15
 *          run time: 1 second
 *          resulting output neuron firing rate: 1 Hz
 *
 * HF case: input firing rate: 25 Hz
 *          weight value from input to output neuron: 25
 *          run time: 1 second
 *          resulting output neuron firing rate: 13 Hz
 *
 * Note that the CARLsim cuba simulations should give the same number of spikes
 * as the Matlab file but with slightly different spike times (offset by 2-3
 * ms). This is probably due to differences in the execution order of simulation
 * functions.
 *
 * Using the Matlab script: runCUBA(runMs, rate, wt)
 * runMs is the number of ms to run the script for (1000 ms in this case)
 * rate is the firing rate of the input neuron (15 or 25 Hz for our cases)
 * wt is the strength of the weight between the input and output neuron (15 or
 * 25 for our cases)
 * The script returns the firing rate and spike times of the output RS neuron.
 */
TEST(CUBA, firingRateVsData) {
	::testing::FLAGS_gtest_death_test_style = "threadsafe";

#ifdef __CPU_ONLY__
	int numModes = 1;
#else
	int numModes = 2;
#endif

	for (int hasHighFiring=0; hasHighFiring<=1; hasHighFiring++) {
		for (int isGPUmode=0; isGPUmode<numModes; isGPUmode++) {
			CARLsim* sim = new CARLsim("CUBA.firingRateVsData",isGPUmode?GPU_MODE:CPU_MODE,SILENT,0,42);
			int g1=sim->createGroup("excit", 1, EXCITATORY_NEURON);
			sim->setNeuronParameters(g1, 0.02f, 0.2f, -65.0f, 8.0f); // RS

			int g0=sim->createSpikeGeneratorGroup("input", 1 , EXCITATORY_NEURON);

			sim->setConductances(false); // make CUBA explicit

			// choose params appropriately (see comments above)
			float wt = hasHighFiring ? 25.0f : 15.0f;
			float inputRate = hasHighFiring ? 25.0f : 50.0f;
			float outputRate = hasHighFiring ? 13.0f : 1.0f; // from Matlab script

			sim->connect(g0, g1, "full", RangeWeight(wt), 1.0f);

			bool spikeAtZero = true;
			PeriodicSpikeGenerator *spkGenG0 = new PeriodicSpikeGenerator(inputRate,spikeAtZero);
			sim->setSpikeGenerator(g0, spkGenG0);

			sim->setupNetwork();

			SpikeMonitor *spkMonG0 = sim->setSpikeMonitor(g0,"NULL");
			SpikeMonitor *spkMonG1 = sim->setSpikeMonitor(g1,"NULL");

			spkMonG0->startRecording();
			spkMonG1->startRecording();
			sim->runNetwork(1,0,false);
			spkMonG0->stopRecording();
			spkMonG1->stopRecording();

			EXPECT_FLOAT_EQ(spkMonG0->getPopMeanFiringRate(), inputRate); // sanity check
			EXPECT_FLOAT_EQ(spkMonG1->getPopMeanFiringRate(), outputRate); // output

			delete spkGenG0;
			delete sim;
		}
	}
}


/*
 * \brief testing CARLsim CUBA output (spike rates) CPU vs GPU
 *
 * This test makes sure that whatever CUBA network is run, both CPU and GPU mode give the exact same output
 * in terms of spike times and spike rates.
 * The total simulation time, input rate, weight, and delay are chosen randomly.
 * Afterwards we make sure that CPU and GPU mode produce the same spike times and spike rates. 
 */
TEST(CUBA, firingRateCPUvsGPU) {
	::testing::FLAGS_gtest_death_test_style = "threadsafe";

	std::vector<std::vector<int> > spkTimesG0CPU, spkTimesG1CPU, spkTimesG0GPU, spkTimesG1GPU;
	float spkRateG0CPU = 0.0f, spkRateG1CPU = 0.0f;
	float wt = 9.440475f;
	float inputRate = 19.0f;
	int runTimeMs = 767;
//	fprintf(stderr,"runTime=%d, delay=%d, wt=%f, input=%f\n",runTimeMs,delay,wt,inputRate);

#ifdef __CPU_ONLY__
	int numModes = 1;
#else
	int numModes = 2;
#endif

	for (int hasIzh4=0; hasIzh4<=1; hasIzh4++) {
		for (int isGPUmode=0; isGPUmode<numModes; isGPUmode++) {
			CARLsim sim("CUBA.firingRateCPUvsGPU",isGPUmode?GPU_MODE:CPU_MODE,SILENT,0,42);
			int g1=sim.createGroup("excit", 1, EXCITATORY_NEURON);
			if (hasIzh4) {
				// 4-param model
				sim.setNeuronParameters(g1, 0.02f, 0.2f, -65.0f, 8.0f); // RS
			} else {
				// 9-param model
				// TODO: add 9-param call for regular-spiking
				sim.setNeuronParameters(g1, 100.0f, 0.7f, -60.0f, -40.0f, 0.03f, -2.0f, 35.0f, -50.0f, 100.0f);//RS
			}

			int g0=sim.createSpikeGeneratorGroup("input", 1 ,EXCITATORY_NEURON);

			sim.setConductances(false); // make CUBA explicit

			sim.connect(g0, g1, "full", RangeWeight(wt), 1.0f, RangeDelay(1));

			bool spikeAtZero = true;
			PeriodicSpikeGenerator spkGenG0(inputRate,spikeAtZero);
			sim.setSpikeGenerator(g0, &spkGenG0);

			sim.setupNetwork();

			SpikeMonitor *spkMonG0 = sim.setSpikeMonitor(g0,"NULL");
			SpikeMonitor *spkMonG1 = sim.setSpikeMonitor(g1,"NULL");

			spkMonG0->startRecording();
			spkMonG1->startRecording();
			sim.runNetwork(runTimeMs/1000,runTimeMs%1000,false);
			spkMonG0->stopRecording();
			spkMonG1->stopRecording();

			if (!isGPUmode) {
				// CPU mode: store spike times and spike rate for future comparison
				spkRateG0CPU = spkMonG0->getPopMeanFiringRate();
				spkRateG1CPU = spkMonG1->getPopMeanFiringRate();
				spkTimesG0CPU = spkMonG0->getSpikeVector2D();
				spkTimesG1CPU = spkMonG1->getSpikeVector2D();
			} else {
				// GPU mode: compare to CPU results

				// do not ASSERT_, otherwise CARLsim will not be correctly deallocated
				// instead, use EXPECT_ and subsequent if-else condition
				bool isRateCorrectG0 = spkMonG0->getPopMeanFiringRate() == spkRateG0CPU;
				bool isRateCorrectG1 = spkMonG1->getPopMeanFiringRate() == spkRateG1CPU;
				EXPECT_TRUE(isRateCorrectG0);
				EXPECT_TRUE(isRateCorrectG1);

				if (isRateCorrectG0 && isRateCorrectG1) {
					spkTimesG0GPU = spkMonG0->getSpikeVector2D();
					spkTimesG1GPU = spkMonG1->getSpikeVector2D();
					bool isSpkSzCorrectG0 = spkTimesG0CPU[0].size() == spkTimesG0GPU[0].size();
					bool isSpkSzCorrectG1 = spkTimesG1CPU[0].size() == spkTimesG1GPU[0].size();
					EXPECT_TRUE(isSpkSzCorrectG0);
					EXPECT_TRUE(isSpkSzCorrectG1);

					if (isSpkSzCorrectG0 && isSpkSzCorrectG1) {
						for (int i=0; i<spkTimesG0CPU[0].size(); i++)
							EXPECT_EQ(spkTimesG0CPU[0][i], spkTimesG0GPU[0][i]);
						for (int i=0; i<spkTimesG1CPU[0].size(); i++)
							EXPECT_EQ(spkTimesG1CPU[0][i], spkTimesG1GPU[0][i]);
					}
				}
			}
		}
	}
}

TEST(CUBA, firingRateVsData9Param) {
        ::testing::FLAGS_gtest_death_test_style = "threadsafe";

	#ifdef __CPU_ONLY__
        int numModes = 1;
	#else
        int numModes = 2;
	#endif

        for (int isGPUmode=0; isGPUmode<numModes; isGPUmode++) {
        	CARLsim* sim = new CARLsim("CUBA.firingRateVsData9Param",isGPUmode?GPU_MODE:CPU_MODE,SILENT,0,42);
                int g1=sim->createGroup("excit", 1, EXCITATORY_NEURON);
		sim->setNeuronParameters(g1, 100.0f, 0.7f, -60.0f, -40.0f, 0.03f, -2.0f, 35.0f, -50.0f, 100.0f);
               	int g0=sim->createSpikeGeneratorGroup("input", 1 , EXCITATORY_NEURON);

                sim->setConductances(false); // make CUBA explicit

		sim->connect(g0, g1, "one-to-one", RangeWeight(0.0f), 1.0f, RangeDelay(1), RadiusRF(-1));
		
                sim->setupNetwork();

		PoissonRate in(1);
                in.setRates(0.0f);
                sim->setSpikeRate(g0, &in);

                SpikeMonitor *spkMonG1 = sim->setSpikeMonitor(g1,"NULL");

                spkMonG1->startRecording();
		sim->setExternalCurrent(g1, 0);
        	sim->runNetwork(0, 100);
        	sim->setExternalCurrent(g1, 70);
        	sim->runNetwork(0, 900);
                spkMonG1->stopRecording();

		//Data is taken from page 275 of Dynamical Systems in Neuroscience The Geometry of Excitability and Bursting
		//by Eugene M. Izhikevich
                EXPECT_FLOAT_EQ(spkMonG1->getPopMeanFiringRate(), 6.0f);
		std::vector<std::vector<int> > spikeTimes = spkMonG1->getSpikeVector2D();
		EXPECT_GT(spikeTimes[0][0], 195);
		EXPECT_LT(spikeTimes[0][0], 215);
		EXPECT_GT(spikeTimes[0][1], 340);
                EXPECT_LT(spikeTimes[0][1], 360);
		EXPECT_GT(spikeTimes[0][2], 485);
                EXPECT_LT(spikeTimes[0][2], 505);
		EXPECT_GT(spikeTimes[0][3], 630);
                EXPECT_LT(spikeTimes[0][3], 650);
		EXPECT_GT(spikeTimes[0][4], 775);
                EXPECT_LT(spikeTimes[0][4], 795);
		EXPECT_GT(spikeTimes[0][5], 925);
                EXPECT_LT(spikeTimes[0][5], 945);		

                delete sim;
        }

}

