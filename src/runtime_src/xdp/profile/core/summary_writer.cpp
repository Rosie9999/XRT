/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include "summary_writer.h"
#include "rt_util.h"
#include "xdp/profile/config.h"
#include "xdp/profile/collection/results.h"
#include "xdp/profile/collection/counters.h"
#include "xdp/profile/device/trace_parser.h"
#include "xdp/profile/writer/base_profile.h"
#include "xdp/profile/writer/base_trace.h"

#include "xcl_perfmon_parameters.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <iomanip>
#include <algorithm>
#include <map>
#include <vector>

namespace xdp {
  // ******************************************
  // Top-Level XDP Profile Summary Writer Class
  // ******************************************
  SummaryWriter::SummaryWriter(ProfileCounters* profileCounters, TraceParser * TraceParserHandle, XDPPluginI* Plugin)
  : mProfileCounters(profileCounters),
    mTraceParserHandle(TraceParserHandle),
    mPluginHandle(Plugin)
  {
  }

  SummaryWriter::~SummaryWriter()
  {
    mFinalCounterResultsMap.clear();
    mRolloverCounterResultsMap.clear();
    mRolloverCountsMap.clear();
    mDeviceBinaryDataSlotsMap.clear();
    mDeviceBinaryCuSlotsMap.clear();
    mDeviceBinaryStrSlotsMap.clear();
  }

  // ***************************************************************************
  // Profile writers
  // ***************************************************************************

  void SummaryWriter::attach(ProfileWriterI* writer)
  {
    std::lock_guard < std::mutex > lock(mLogMutex);
    auto itr = std::find(mProfileWriters.begin(), mProfileWriters.end(), writer);
    if (itr == mProfileWriters.end())
      mProfileWriters.push_back(writer);
  }

  void SummaryWriter::detach(ProfileWriterI* writer)
  {
    std::lock_guard < std::mutex > lock(mLogMutex);
    auto itr = std::find(mProfileWriters.begin(), mProfileWriters.end(), writer);
    if (itr != mProfileWriters.end())
      mProfileWriters.erase(itr);
  }

  // ***************************************************************************
  // Log device counters
  // ***************************************************************************

  void SummaryWriter::logDeviceCounters(std::string deviceName, std::string binaryName, uint32_t programID,
       xclPerfMonType type, xclCounterResults& counterResults, uint64_t timeNsec, bool firstReadAfterProgram)
  {
    // Number of monitor slots
    uint32_t numSlots = 0;
    std::string key = deviceName + "|" + binaryName;
    std::string slotName = "";

    XDP_LOG("logDeviceCounters: first read = %d, device: %s\n", firstReadAfterProgram, deviceName.c_str());

    // If not already defined, zero out rollover values for this device
    if (mFinalCounterResultsMap.find(key) == mFinalCounterResultsMap.end()) {
      mFinalCounterResultsMap[key] = counterResults;

      xclCounterResults rolloverResults;
      memset(&rolloverResults, 0, sizeof(xclCounterResults));
      //rolloverResults.NumSlots = counterResults.NumSlots;
      mRolloverCounterResultsMap[key] = rolloverResults;
      mRolloverCountsMap[key] = rolloverResults;
    }
    else {
      /*
       * Log SPM Counters
       */
      numSlots = mPluginHandle->getProfileNumberSlots(XCL_PERF_MON_MEMORY, deviceName);
      // Traverse all monitor slots (host and all CU ports)
      bool deviceDataExists = (mDeviceBinaryDataSlotsMap.find(key) == mDeviceBinaryDataSlotsMap.end()) ? false : true;
      for (unsigned int s=0; s < numSlots; ++s) {
        mPluginHandle->getProfileSlotName(XCL_PERF_MON_MEMORY, deviceName, s, slotName);
        if (!deviceDataExists) {
          mDeviceBinaryDataSlotsMap[key].push_back(slotName);
          auto p = mPluginHandle->getProfileSlotProperties(XCL_PERF_MON_MEMORY, deviceName, s);
          mDataSlotsPropertiesMap[key].push_back(p);
        }
        uint64_t prevWriteBytes   = mFinalCounterResultsMap[key].WriteBytes[s];
        uint64_t prevReadBytes    = mFinalCounterResultsMap[key].ReadBytes[s];
        uint64_t prevWriteTranx   = mFinalCounterResultsMap[key].WriteTranx[s];
        uint64_t prevReadTranx    = mFinalCounterResultsMap[key].ReadTranx[s];
        uint64_t prevWriteLatency = mFinalCounterResultsMap[key].WriteLatency[s];
        uint64_t prevReadLatency  = mFinalCounterResultsMap[key].ReadLatency[s];
        uint64_t prevReadBusyCycles   = mFinalCounterResultsMap[key].ReadBusyCycles[s];
        uint64_t prevWriteBusyCycles  = mFinalCounterResultsMap[key].WriteBusyCycles[s];

        // Check for rollover of byte counters; if detected, add 2^32
        // Otherwise, if first read after program with binary, then capture bytes from previous xclbin
        if (!firstReadAfterProgram) {
          if (counterResults.WriteBytes[s] < prevWriteBytes)
            mRolloverCountsMap[key].WriteBytes[s]    += 1;
          if (counterResults.ReadBytes[s] < prevReadBytes)
            mRolloverCountsMap[key].ReadBytes[s]     += 1;
          if (counterResults.WriteTranx[s] < prevWriteTranx)
            mRolloverCountsMap[key].WriteTranx[s]    += 1;
          if (counterResults.ReadTranx[s] < prevReadTranx)
            mRolloverCountsMap[key].ReadTranx[s]     += 1;
          if (counterResults.WriteLatency[s] < prevWriteLatency)
            mRolloverCountsMap[key].WriteLatency[s]  += 1;
          if (counterResults.ReadLatency[s] < prevReadLatency)
            mRolloverCountsMap[key].ReadLatency[s]   += 1;
          if (counterResults.ReadBusyCycles[s] < prevReadBusyCycles)
            mRolloverCountsMap[key].ReadBusyCycles[s]  += 1;
          if (counterResults.WriteBusyCycles[s] < prevWriteBusyCycles)
            mRolloverCountsMap[key].WriteBusyCycles[s] += 1;
        }
        else {
          mRolloverCounterResultsMap[key].WriteBytes[s]    += prevWriteBytes;
          mRolloverCounterResultsMap[key].ReadBytes[s]     += prevReadBytes;
          mRolloverCounterResultsMap[key].WriteTranx[s]    += prevWriteTranx;
          mRolloverCounterResultsMap[key].ReadTranx[s]     += prevReadTranx;
          mRolloverCounterResultsMap[key].WriteLatency[s]  += prevWriteLatency;
          mRolloverCounterResultsMap[key].ReadLatency[s]   += prevReadLatency;
          mRolloverCounterResultsMap[key].ReadBusyCycles[s] += prevReadBusyCycles;
          mRolloverCounterResultsMap[key].WriteBusyCycles[s] += prevWriteBusyCycles;
        }
      }
      /*
       * Log SAM Counters
       */
      numSlots = mPluginHandle->getProfileNumberSlots(XCL_PERF_MON_ACCEL, deviceName);
      for (unsigned int s=0; s < numSlots; ++s) {
        uint64_t prevCuExecCount       = mFinalCounterResultsMap[key].CuExecCount[s];
        uint64_t prevCuExecCycles      = mFinalCounterResultsMap[key].CuExecCycles[s];
        uint64_t prevCuBusyCycles      = mFinalCounterResultsMap[key].CuBusyCycles[s];
        uint64_t prevCuStallExtCycles  = mFinalCounterResultsMap[key].CuStallExtCycles[s];
        uint64_t prevCuStallIntCycles  = mFinalCounterResultsMap[key].CuStallIntCycles[s];
        uint64_t prevCuStallStrCycles  = mFinalCounterResultsMap[key].CuStallStrCycles[s];
        if (!firstReadAfterProgram) {
          if (counterResults.CuExecCycles[s] < prevCuExecCycles)
            mRolloverCountsMap[key].CuExecCycles[s]     += 1;
          if (counterResults.CuBusyCycles[s] < prevCuBusyCycles)
            mRolloverCountsMap[key].CuBusyCycles[s]     += 1;
          if (counterResults.CuStallExtCycles[s] < prevCuStallExtCycles)
            mRolloverCountsMap[key].CuStallExtCycles[s] += 1;
          if (counterResults.CuStallIntCycles[s] < prevCuStallIntCycles)
            mRolloverCountsMap[key].CuStallIntCycles[s] += 1;
          if (counterResults.CuStallStrCycles[s] < prevCuStallStrCycles)
            mRolloverCountsMap[key].CuStallStrCycles[s] += 1;
        }
        else {
          mRolloverCounterResultsMap[key].CuExecCount[s]      += prevCuExecCount;
          mRolloverCounterResultsMap[key].CuExecCycles[s]     += prevCuExecCycles;
          mRolloverCounterResultsMap[key].CuBusyCycles[s]     += prevCuBusyCycles;
          mRolloverCounterResultsMap[key].CuStallExtCycles[s] += prevCuStallExtCycles;
          mRolloverCounterResultsMap[key].CuStallIntCycles[s] += prevCuStallIntCycles;
          mRolloverCounterResultsMap[key].CuStallStrCycles[s] += prevCuStallStrCycles;
        }
      }
      /*
       * Streaming IP Counters are 64 bit and unlikely to roll over
       */
      numSlots = mPluginHandle->getProfileNumberSlots(XCL_PERF_MON_STR, deviceName);
      deviceDataExists = (mDeviceBinaryStrSlotsMap.find(key) == mDeviceBinaryStrSlotsMap.end()) ? false : true;
      for (unsigned int s=0; s < numSlots; ++s) {
        mPluginHandle->getProfileSlotName(XCL_PERF_MON_STR, deviceName, s, slotName);
        if (!deviceDataExists)
          mDeviceBinaryStrSlotsMap[key].push_back(slotName);
      }
      mFinalCounterResultsMap[key] = counterResults;
    }
    /*
     * Update Stats Database
     */
    uint32_t kernelClockMhz = mPluginHandle->getKernelClockFreqMHz(deviceName);
    double deviceCyclesMsec = kernelClockMhz * 1000.0 ;
    numSlots = mPluginHandle->getProfileNumberSlots(XCL_PERF_MON_ACCEL, deviceName);
    std::string cuName = "";
    std::string kernelName ="";
    bool deviceDataExists = (mDeviceBinaryCuSlotsMap.find(key) == mDeviceBinaryCuSlotsMap.end()) ? false : true;
    xclCounterResults rolloverResults = mRolloverCounterResultsMap.at(key);
    xclCounterResults rolloverCounts = mRolloverCountsMap.at(key);
    for (unsigned int s=0; s < numSlots; ++s) {
      mPluginHandle->getProfileSlotName(XCL_PERF_MON_ACCEL, deviceName, s, cuName);
      mPluginHandle->getProfileKernelName(deviceName, cuName, kernelName);
      if (!deviceDataExists)
        mDeviceBinaryCuSlotsMap[key].push_back(cuName);
      uint32_t cuExecCount = counterResults.CuExecCount[s] + rolloverResults.CuExecCount[s];
      uint64_t cuExecCycles = counterResults.CuExecCycles[s] + rolloverResults.CuExecCycles[s]
                                + (rolloverCounts.CuExecCycles[s] * 4294967296UL);
      uint64_t cuBusyCycles = counterResults.CuBusyCycles[s] + rolloverResults.CuBusyCycles[s]
                                + (rolloverCounts.CuBusyCycles[s] * 4294967296UL);
      uint32_t cuMaxExecCycles  = counterResults.CuMaxExecCycles[s];
      uint32_t cuMinExecCycles  = counterResults.CuMinExecCycles[s];
      uint64_t cuMaxParallelIter = counterResults.CuMaxParallelIter[s];
      double cuRunTimeMsec = (double) cuBusyCycles / deviceCyclesMsec;
      double cuRunTimeAvgMsec = (double) (cuExecCycles / deviceCyclesMsec) / cuExecCount;
      double cuMaxExecCyclesMsec = (double) cuMaxExecCycles / deviceCyclesMsec;
      double cuMinExecCyclesMsec = (double) cuMinExecCycles / deviceCyclesMsec;
      uint32_t isDataflow = mPluginHandle->isAPCtrlChain(deviceName, cuName) ? 1 : 0;
      std::string binaryInstance = binaryName + std::to_string(programID);
      //XDP_LOG("[RT_PROFILE] cuName : %s exec cycles : %d runtime %f \n", cuName.c_str(), cuExecCycles, cuRunTimeMsec);
      // Don't log if not a valid stat
      if (cuMaxParallelIter > 0)
        mProfileCounters->logComputeUnitStats(cuName, kernelName, cuRunTimeMsec,
                                              cuRunTimeAvgMsec, cuMaxExecCyclesMsec,
                                              cuMinExecCyclesMsec, cuExecCount, kernelClockMhz,
                                              isDataflow, cuMaxParallelIter, deviceName, binaryInstance);
    }
#ifdef XDP_VERBOSE
    if (this->isTimelineTraceFileOn()) {
      static uint32_t sampleNum = 0;
      double timeStamp = getTimestampMsec(timeNsec);

      std::lock_guard < std::mutex > lock(mLogMutex);
      for (auto w : TraceWriters) {
        w->writeDeviceCounters(type, counterResults, timeStamp, sampleNum, firstReadAfterProgram);
      }
      sampleNum++;
    }
#endif
  }

  // ***************************************************************************
  // Table writers for profile summary
  // ***************************************************************************

  void SummaryWriter::writeProfileSummary(RTProfile* profile) {
    for (auto w : mProfileWriters) {
      w->writeSummary(profile);
    }
  }

  void SummaryWriter::writeAPISummary(ProfileWriterI* writer) const
  {
    mProfileCounters->writeAPISummary(writer);
  }

  void SummaryWriter::writeKernelSummary(ProfileWriterI* writer) const
  {
    mProfileCounters->writeKernelSummary(writer);
  }

  void SummaryWriter::writeComputeUnitSummary(ProfileWriterI* writer) const
  {
    mProfileCounters->writeComputeUnitSummary(writer);
  }

  void SummaryWriter::writeAcceleratorSummary(ProfileWriterI* writer) const
  {
    mProfileCounters->writeAcceleratorSummary(writer);
  }

  void SummaryWriter::writeTransferSummary(ProfileWriterI* writer, RTUtil::e_monitor_type monitorType) const
  {
    std::string deviceName;

    // Get maximum throughput rates
    double readMaxBandwidthMBps = 0.0;
    double writeMaxBandwidthMBps = 0.0;
    if (mPluginHandle->getFlowMode() != xdp::RTUtil::CPU
        && mPluginHandle->getFlowMode() != xdp::RTUtil::COSIM_EM) {
      readMaxBandwidthMBps = mPluginHandle->getReadMaxBandwidthMBps();
      writeMaxBandwidthMBps = mPluginHandle->getWriteMaxBandwidthMBps();
    }

    //
    // Host transfers
    //
    // Catch host buffer transfers (reported on aggregated basis)
    // NOTE: the actual statistics reported come from BufferReadStat and BufferWriteStat in ProfileCounters
    if (monitorType == RTUtil::MON_HOST_DYNAMIC) {
      mProfileCounters->writeTransferSummary(writer, deviceName, monitorType,  true, 0, 0, 0, 0,  readMaxBandwidthMBps);
      mProfileCounters->writeTransferSummary(writer, deviceName, monitorType, false, 0, 0, 0, 0, writeMaxBandwidthMBps);
      return;
    }

    auto tp = mTraceParserHandle;

    std::string monitorName;
    RTUtil::monitorTypeToString(monitorType, monitorName);

    double totalReadTimeMsec  = 0;
    double totalWriteTimeMsec = 0;

    /** Old way of calculating bandwidth is not compatible with AIM hw registers
     * We show N/A in Summary if these registers are absent

    if (monitorType == RTUtil::MON_SHELL_KDMA) {
      totalReadTimeMsec  += mProfileCounters->getBufferTransferTotalTime(RTUtil::COPY_BUFFER);
      totalWriteTimeMsec += mProfileCounters->getBufferTransferTotalTime(RTUtil::COPY_BUFFER);
    }
    else if (monitorType == RTUtil::MON_SHELL_P2P) {
      totalReadTimeMsec  += mProfileCounters->getBufferTransferTotalTime(RTUtil::COPY_BUFFER_P2P);
      totalReadTimeMsec  += mProfileCounters->getBufferTransferTotalTime(RTUtil::READ_BUFFER_P2P);

      totalWriteTimeMsec += mProfileCounters->getBufferTransferTotalTime(RTUtil::COPY_BUFFER_P2P);
      totalWriteTimeMsec += mProfileCounters->getBufferTransferTotalTime(RTUtil::WRITE_BUFFER_P2P);
    } else {
      totalReadTimeMsec  += mProfileCounters->getBufferTransferTotalTime(RTUtil::READ_BUFFER);
      totalWriteTimeMsec += mProfileCounters->getBufferTransferTotalTime(RTUtil::WRITE_BUFFER);
    }

    */

    //
    // Shell monitors: KDMA/XDMA/P2P
    //
    // Traverse each device and report KDMA/XDMA/P2P counters (if available)
    auto iter = mFinalCounterResultsMap.begin();
    for (; iter != mFinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      deviceName = key.substr(0, key.find_first_of("|"));

      // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverCounts;
      if (mRolloverCountsMap.find(key) != mRolloverCountsMap.end())
        rolloverCounts = mRolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));

      uint64_t totalReadBytes    = 0;
      uint64_t totalWriteBytes   = 0;
      uint64_t totalReadTranx    = 0;
      uint64_t totalWriteTranx   = 0;
      uint64_t totalReadLatency  = 0;
      uint64_t totalWriteLatency = 0;
      uint64_t totalReadBusyCycles = 0;
      uint64_t totalWriteBusyCycles = 0;

      // Traverse all slots to find shell monitors
      uint32_t numSlots = mPluginHandle->getProfileNumberSlots(XCL_PERF_MON_MEMORY, deviceName);

      for (uint32_t s=0; s < numSlots; s++) {
        // Make sure it's the shell monitor we're looking for
        // NOTE: properties and name must match
        uint32_t properties = mPluginHandle->getProfileSlotProperties(XCL_PERF_MON_MEMORY, deviceName, s);
        if (!(properties & XAIM_HOST_PROPERTY_MASK))
          continue;

        std::string slotName;
        mPluginHandle->getProfileSlotName(XCL_PERF_MON_MEMORY, deviceName, s, slotName);
        if (slotName.find(monitorName) == std::string::npos)
          continue;

        // We found one! Now add it to totals
        totalReadBytes += counterResults.ReadBytes[s]
                          + (rolloverCounts.ReadBytes[s] * 4294967296UL);
        totalWriteBytes += counterResults.WriteBytes[s]
                          + (rolloverCounts.WriteBytes[s] * 4294967296UL);
        totalReadTranx += counterResults.ReadTranx[s]
                          + (rolloverCounts.ReadTranx[s] * 4294967296UL);
        totalWriteTranx += counterResults.WriteTranx[s]
                          + (rolloverCounts.WriteTranx[s] * 4294967296UL);
        // Total transfer time = sum of all tranx latencies
        // msec = cycles / (1000 * (Mcycles/sec))
        totalReadLatency += counterResults.ReadLatency[s]
                            + (rolloverCounts.ReadLatency[s] * 4294967296UL);
        totalWriteLatency += counterResults.WriteLatency[s]
                            + (rolloverCounts.WriteLatency[s] * 4294967296UL);
        totalReadBusyCycles += counterResults.ReadBusyCycles[s]
                              + (rolloverCounts.ReadBusyCycles[s] * 4294967296UL);
        totalWriteBusyCycles += counterResults.WriteBusyCycles[s]
                               + (rolloverCounts.WriteBusyCycles[s] * 4294967296UL);
      }

      double totalReadLatencyNsec  = (1000.0 * totalReadLatency)  / tp->getDeviceClockFreqMHz();
      double totalWriteLatencyNsec = (1000.0 * totalWriteLatency) / tp->getDeviceClockFreqMHz();
      totalReadTimeMsec = totalReadBusyCycles / (1000.0 * tp->getDeviceClockFreqMHz());
      totalWriteTimeMsec = totalWriteBusyCycles / (1000.0 * tp->getDeviceClockFreqMHz());

      // Monitoring of KDMA/XDMA/P2P is reported on per-device basis
      // NOTE: don't show if no transfers were recorded
      if (totalReadTranx > 0) {
        mProfileCounters->writeTransferSummary(writer, deviceName, monitorType, true,  totalReadBytes,
            totalReadTranx, totalReadLatencyNsec, totalReadTimeMsec, readMaxBandwidthMBps);
      }
      if (totalWriteTranx > 0) {
        mProfileCounters->writeTransferSummary(writer, deviceName, monitorType, false, totalWriteBytes,
            totalWriteTranx, totalWriteLatencyNsec, totalWriteTimeMsec, writeMaxBandwidthMBps);
      }
    }
  }

  void SummaryWriter::writeStallSummary(ProfileWriterI* writer) const
  {
    auto tp = mTraceParserHandle;

    auto iter = mFinalCounterResultsMap.begin();
    double deviceCyclesMsec = (tp->getDeviceClockFreqMHz() * 1000.0);
    for (; iter != mFinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (mDeviceBinaryCuSlotsMap.find(key) == mDeviceBinaryCuSlotsMap.end())
        continue;

    // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverResults;
      if (mRolloverCounterResultsMap.find(key) != mRolloverCounterResultsMap.end())
        rolloverResults = mRolloverCounterResultsMap.at(key);
      else
        memset(&rolloverResults, 0, sizeof(xclCounterResults));

      xclCounterResults rolloverCounts;
      if (mRolloverCountsMap.find(key) != mRolloverCountsMap.end())
        rolloverCounts = mRolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));

      std::string cuName = "";

      uint32_t numSlots = mDeviceBinaryCuSlotsMap.at(key).size();
      for (unsigned int s=0; s < numSlots; ++s) {
        cuName = mDeviceBinaryCuSlotsMap.at(key)[s];
        uint32_t cuExecCount = counterResults.CuExecCount[s] + rolloverResults.CuExecCount[s];
        uint64_t cuExecCycles = counterResults.CuExecCycles[s] + rolloverResults.CuExecCycles[s]
                                  + (rolloverCounts.CuExecCycles[s] * 4294967296UL);
        uint64_t cuStallExtCycles = counterResults.CuStallExtCycles[s] + rolloverResults.CuStallExtCycles[s]
                                  + (rolloverCounts.CuStallExtCycles[s] * 4294967296UL);
        uint64_t cuStallStrCycles = counterResults.CuStallStrCycles[s] + rolloverResults.CuStallStrCycles[s]
                                  + (rolloverCounts.CuStallStrCycles[s] * 4294967296UL);
        uint64_t cuStallIntCycles = counterResults.CuStallIntCycles[s] + rolloverResults.CuStallIntCycles[s]
                                  + (rolloverCounts.CuStallIntCycles[s] * 4294967296UL);
        double cuRunTimeMsec = (double) cuExecCycles / deviceCyclesMsec;
        double cuStallExt =    (double) cuStallExtCycles / deviceCyclesMsec;
        double cuStallStr =    (double) cuStallStrCycles / deviceCyclesMsec;
        double cuStallInt =    (double) cuStallIntCycles / deviceCyclesMsec;
        writer->writeStallSummary(cuName, cuExecCount, cuRunTimeMsec,
                                  cuStallExt, cuStallStr, cuStallInt);
      }
    }
  }

  void SummaryWriter::writeKernelStreamSummary(ProfileWriterI* writer)
  {
    auto iter = mFinalCounterResultsMap.begin();
    for (; iter != mFinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (mDeviceBinaryStrSlotsMap.find(key) == mDeviceBinaryStrSlotsMap.end())
        continue;

      // Get results
      xclCounterResults counterResults = iter->second;
      uint32_t numSlots = mDeviceBinaryStrSlotsMap.at(key).size();

      std::string cuPortName;
      std::string masterPortName;
      std::string slavePortName;
      std::string masterArgNames = FIELD_NOT_APPLICABLE;
      std::string slaveArgNames = FIELD_NOT_APPLICABLE;
      std::size_t cuFound = 0;
      std::size_t masterSlaveFound = 0;
      for (unsigned int s=0; s < numSlots; ++s) {
        cuPortName = mDeviceBinaryStrSlotsMap.at(key)[s];
        masterSlaveFound = cuPortName.find(IP_LAYOUT_SEP);
        // Debug IP format : "MasterName-SlaveName"
        if (masterSlaveFound == std::string::npos)
          return;
        masterPortName = cuPortName.substr(0, masterSlaveFound);
        slavePortName = cuPortName.substr(masterSlaveFound + 1);
        cuFound = masterPortName.find_first_of("/");
        // Look for arguments if not HOST or PIPE
        if (cuFound != std::string::npos) {
          auto cu = masterPortName.substr(0, cuFound);
          auto port = masterPortName.substr(cuFound+1);
          std::string placeholder;
          mPluginHandle->getArgumentsBank(deviceName, cu, port, masterArgNames, placeholder);
        }
        cuFound = slavePortName.find_first_of("/");
        if (cuFound != std::string::npos) {
          auto cu = slavePortName.substr(0, cuFound);
          auto port = slavePortName.substr(cuFound+1);
          std::string placeholder;
          mPluginHandle->getArgumentsBank(deviceName, cu, port, slaveArgNames, placeholder);
        }

        uint64_t strNumTranx =     counterResults.StrNumTranx[s];
        uint64_t strBusyCycles =   counterResults.StrBusyCycles[s];
        uint64_t strDataBytes =    counterResults.StrDataBytes[s];
        uint64_t strStallCycles =  counterResults.StrStallCycles[s];
        uint64_t strStarveCycles = counterResults.StrStarveCycles[s];
        // Skip ports without activity
        if (strBusyCycles <= 0 || strNumTranx == 0)
          continue;

        double transferTimeUsec = strBusyCycles / mTraceParserHandle->getDeviceClockFreqMHz();
        double transferRateMBps = (transferTimeUsec == 0) ? 0.0 :
                                  (strDataBytes / transferTimeUsec);

        double avgSize    =  (double) strDataBytes / (double) strNumTranx * 0.001 ;
        double linkStarve = (double) strStarveCycles / (double) strBusyCycles * 100.0;
        double linkStall =  (double) strStallCycles / (double) strBusyCycles * 100.0;
        double linkUtil =  100.0 - linkStarve - linkStall;
        writer->writeKernelStreamSummary(deviceName, masterPortName, masterArgNames,
                                         slavePortName, slaveArgNames, strNumTranx,
                                         transferRateMBps, avgSize, linkUtil, linkStarve,
                                         linkStall);
      }
    }
  }

  void SummaryWriter::writeKernelTransferSummary(ProfileWriterI* writer)
  {
    auto iter = mFinalCounterResultsMap.begin();
    for (; iter != mFinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (mDeviceBinaryDataSlotsMap.find(key) == mDeviceBinaryDataSlotsMap.end())
        continue;

      // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverResults;
      if (mRolloverCounterResultsMap.find(key) != mRolloverCounterResultsMap.end())
        rolloverResults = mRolloverCounterResultsMap.at(key);
      else
        memset(&rolloverResults, 0, sizeof(xclCounterResults));

      xclCounterResults rolloverCounts;
      if (mRolloverCountsMap.find(key) != mRolloverCountsMap.end())
        rolloverCounts = mRolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));

      // Number of monitor slots
      uint32_t numSlots = mDeviceBinaryDataSlotsMap.at(key).size();

      // Total kernel time = sum of all kernel executions
      //double totalKernelTimeMsec = mProfileCounters->getTotalKernelExecutionTime(deviceName);
      double maxTransferRateMBps = getGlobalMemoryMaxBandwidthMBps();

      for (unsigned s = 0; s < numSlots; ++s) {
        if (mDataSlotsPropertiesMap.at(key)[s] & XAIM_HOST_PROPERTY_MASK)
          continue;

        std::string cuPortName = mDeviceBinaryDataSlotsMap.at(key)[s];
        std::string cuName = cuPortName.substr(0, cuPortName.find_first_of("/"));
        std::string portName = cuPortName.substr(cuPortName.find_first_of("/")+1);
        //std::transform(portName.begin(), portName.end(), portName.begin(), ::tolower);

        std::string memoryName;
        std::string argNames;
        mPluginHandle->getArgumentsBank(deviceName, cuName, portName, argNames, memoryName);

        uint64_t totalReadBytes    = counterResults.ReadBytes[s] + rolloverResults.ReadBytes[s]
                                     + (rolloverCounts.ReadBytes[s] * 4294967296UL);
        uint64_t totalWriteBytes   = counterResults.WriteBytes[s] + rolloverResults.WriteBytes[s]
                                     + (rolloverCounts.WriteBytes[s] * 4294967296UL);
        uint64_t totalReadTranx    = counterResults.ReadTranx[s] + rolloverResults.ReadTranx[s]
                                     + (rolloverCounts.ReadTranx[s] * 4294967296UL);
        uint64_t totalWriteTranx   = counterResults.WriteTranx[s] + rolloverResults.WriteTranx[s]
                                     + (rolloverCounts.WriteTranx[s] * 4294967296UL);
        // Total tx times for write and read channels
        uint64_t totalReadBusyCycles   = counterResults.ReadBusyCycles[s] + rolloverResults.ReadBusyCycles[s]
                                     + (rolloverCounts.ReadBusyCycles[s] * 4294967296UL);
        double totalReadTimeMsec = totalReadBusyCycles / (1000.0 * mTraceParserHandle->getDeviceClockFreqMHz());
        uint64_t totalWriteBusyCycles   = counterResults.WriteBusyCycles[s] + rolloverResults.WriteBusyCycles[s]
                                     + (rolloverCounts.WriteBusyCycles[s] * 4294967296UL);
        double totalWriteTimeMsec = totalWriteBusyCycles / (1000.0 * mTraceParserHandle->getDeviceClockFreqMHz());

        // Total latency = sum of all tranx latencies
        // msec = cycles / (1000 * (Mcycles/sec))
        uint64_t totalReadLatency  = counterResults.ReadLatency[s] + rolloverResults.ReadLatency[s]
                                     + (rolloverCounts.ReadLatency[s] * 4294967296UL);
        double totalReadLatencyMsec   = totalReadLatency / (1000.0 * mTraceParserHandle->getDeviceClockFreqMHz());
        uint64_t totalWriteLatency = counterResults.WriteLatency[s] + rolloverResults.WriteLatency[s]
                                     + (rolloverCounts.WriteLatency[s] * 4294967296UL);
        double totalWriteLatencyMsec  = totalWriteLatency / (1000.0 * mTraceParserHandle->getDeviceClockFreqMHz());

        XDP_LOG("writeKernelTransferSummary: s=%d, reads=%d, writes=%d, %s time = %f msec\n",
            s, totalReadTranx, totalWriteTranx, cuName.c_str(), totalCUTimeMsec);

        // First do READ, then WRITE
        if (totalReadTranx > 0) {
          mProfileCounters->writeKernelTransferSummary(writer, deviceName, cuPortName, argNames, memoryName,
            true,  totalReadBytes, totalReadTranx, totalReadTimeMsec, totalReadLatencyMsec, maxTransferRateMBps);
        }
        if (totalWriteTranx > 0) {
          mProfileCounters->writeKernelTransferSummary(writer, deviceName, cuPortName, argNames, memoryName,
            false, totalWriteBytes, totalWriteTranx, totalWriteTimeMsec, totalWriteLatencyMsec, maxTransferRateMBps);
        }
      }
    }
  }

  void SummaryWriter::writeTopKernelSummary(ProfileWriterI* writer) const
  {
    mProfileCounters->writeTopKernelSummary(writer);
  }

  void SummaryWriter::writeTopHardwareSummary(ProfileWriterI* writer) const
  {
    mProfileCounters->writeTopHardwareSummary(writer);
  }

  void SummaryWriter::writeTopKernelTransferSummary(ProfileWriterI* writer) const
  {
    // Iterate over all devices
    auto iter = mFinalCounterResultsMap.begin();
    for (; iter != mFinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (mDeviceBinaryDataSlotsMap.find(key) == mDeviceBinaryDataSlotsMap.end())
        continue;

      // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverResults;
      if (mRolloverCounterResultsMap.find(key) != mRolloverCounterResultsMap.end())
        rolloverResults = mRolloverCounterResultsMap.at(key);
      else
        memset(&rolloverResults, 0, sizeof(xclCounterResults));

      xclCounterResults rolloverCounts;
      if (mRolloverCountsMap.find(key) != mRolloverCountsMap.end())
        rolloverCounts = mRolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));

      // Number of monitor slots
      uint32_t numSlots = mDeviceBinaryDataSlotsMap.at(key).size();
      double maxTransferRateMBps = getGlobalMemoryMaxBandwidthMBps();

      // Maximum bytes per AXI data transfer
      // NOTE: this assumes the entire global memory bit width with a burst of 256 (max burst length of AXI4)
      //       AXI standard also limits a transfer to 4K total bytes
      uint32_t maxBytesPerTransfer = (mTraceParserHandle->getGlobalMemoryBitWidth() / 8) * 256;
      if (maxBytesPerTransfer > 4096)
        maxBytesPerTransfer = 4096;

      // Gather unique names of monitored CUs on this device
      std::map<std::string, uint64_t> cuNameTranxMap;
      for (unsigned s=0; s < numSlots; ++s) {
        if (mDataSlotsPropertiesMap.at(key)[s] & XAIM_HOST_PROPERTY_MASK)
          continue;

        std::string cuPortName = mDeviceBinaryDataSlotsMap.at(key)[s];
        std::string cuName = cuPortName.substr(0, cuPortName.find_first_of("/"));
        cuNameTranxMap[cuName] = 0;
      }

      // Get their total tranx counts
      auto cuIter = cuNameTranxMap.begin();
      for (; cuIter != cuNameTranxMap.end(); ++cuIter) {
        std::string cuName = cuIter->first;

        uint64_t totalReadTranx  = 0;
        uint64_t totalWriteTranx = 0;
        for (unsigned s=0; s < numSlots; ++s) {
          if (mDataSlotsPropertiesMap.at(key)[s] & XAIM_HOST_PROPERTY_MASK)
            continue;

          std::string cuPortName = mDeviceBinaryDataSlotsMap.at(key)[s];
          std::string cuSlotName = cuPortName.substr(0, cuPortName.find_first_of("/"));

          if (cuSlotName == cuName) {
            totalReadTranx  += counterResults.ReadTranx[s] + rolloverResults.ReadTranx[s]
                               + (rolloverCounts.ReadTranx[s] * 4294967296UL);
            totalWriteTranx += counterResults.WriteTranx[s] + rolloverResults.WriteTranx[s]
                               + (rolloverCounts.WriteTranx[s] * 4294967296UL);
          }
        }

        cuNameTranxMap[cuName] = (totalReadTranx + totalWriteTranx);
      }

      // Sort the CUs by their tranx count
      std::vector<std::pair<std::string, uint64_t>> cuPairs(cuNameTranxMap.begin(),
          cuNameTranxMap.end());
      std::sort(cuPairs.begin(), cuPairs.end(),
          [](const std::pair<std::string, uint64_t>& A, const std::pair<std::string, uint64_t>& B) {
               return (A.second > B.second);
             });

      // Now report them in order of total tranx counts
      for (const auto &pair : cuPairs) {
        std::string cuName = pair.first;

        uint64_t totalReadBytes  = 0;
        uint64_t totalWriteBytes = 0;
        uint64_t totalReadTranx  = 0;
        uint64_t totalWriteTranx = 0;
        uint64_t totalReadBusyCycles = 0;
        uint64_t totalWriteBusyCycles = 0;
        for (unsigned s=0; s < numSlots; ++s) {
          if (mDataSlotsPropertiesMap.at(key)[s] & XAIM_HOST_PROPERTY_MASK)
            continue;

          std::string cuPortName = mDeviceBinaryDataSlotsMap.at(key)[s];
          std::string cuSlotName = cuPortName.substr(0, cuPortName.find_first_of("/"));

          if (cuSlotName == cuName) {
            totalReadBytes  += counterResults.ReadBytes[s] + rolloverResults.ReadBytes[s]
                               + (rolloverCounts.ReadBytes[s] * 4294967296UL);
            totalWriteBytes += counterResults.WriteBytes[s] + rolloverResults.WriteBytes[s]
                               + (rolloverCounts.WriteBytes[s] * 4294967296UL);
            totalReadTranx  += counterResults.ReadTranx[s] + rolloverResults.ReadTranx[s]
                               + (rolloverCounts.ReadTranx[s] * 4294967296UL);
            totalWriteTranx += counterResults.WriteTranx[s] + rolloverResults.WriteTranx[s]
                               + (rolloverCounts.WriteTranx[s] * 4294967296UL);
            totalReadBusyCycles += counterResults.ReadBusyCycles[s] + rolloverResults.ReadBusyCycles[s]
                                  + (rolloverCounts.ReadBusyCycles[s] * 4294967296UL);
            totalWriteBusyCycles += counterResults.WriteBusyCycles[s] + rolloverResults.WriteBusyCycles[s]
                                  + (rolloverCounts.WriteBusyCycles[s] * 4294967296UL);
          }
        }

        double totalReadTimeMsec = totalReadBusyCycles / (1000.0 * mTraceParserHandle->getDeviceClockFreqMHz()) ;
        double totalWriteTimeMsec = totalWriteBusyCycles / (1000.0 * mTraceParserHandle->getDeviceClockFreqMHz());

        mProfileCounters->writeTopKernelTransferSummary(writer, deviceName, cuName, totalWriteBytes,
            totalReadBytes, totalWriteTranx, totalReadTranx, totalWriteTimeMsec, totalReadTimeMsec,
            maxBytesPerTransfer, maxTransferRateMBps);
      }
    }
  }

    // Max. achievable bandwidth between kernels and DDR global memory = 60% of 10.7 GBps for PCIe Gen 3
  // TODO: this should come from benchmarking results
  double SummaryWriter::getGlobalMemoryMaxBandwidthMBps() const
  {
    double maxBandwidthMBps =
        0.6 * (mTraceParserHandle->getGlobalMemoryBitWidth() / 8) * mTraceParserHandle->getGlobalMemoryClockFreqMHz();
    return maxBandwidthMBps;
  }

  void SummaryWriter::writeDeviceTransferSummary(ProfileWriterI* writer) const
  {
    mProfileCounters->writeDeviceTransferSummary(writer, true);
    mProfileCounters->writeDeviceTransferSummary(writer, false);
  }

  void SummaryWriter::writeTopDataTransferSummary(ProfileWriterI* writer, bool isRead) const
  {
    mProfileCounters->writeTopDataTransferSummary(writer, isRead);
  }

  void SummaryWriter::writeTopDeviceTransferSummary(ProfileWriterI* writer, bool isRead) const
  {
    mProfileCounters->writeTopDeviceTransferSummary(writer, isRead);
  }

} // xdp
