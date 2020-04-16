// EnergyPlus, Copyright (c) 1996-2020, The Board of Trustees of the University of Illinois,
// The Regents of the University of California, through Lawrence Berkeley National Laboratory
// (subject to receipt of any required approvals from the U.S. Dept. of Energy), Oak Ridge
// National Laboratory, managed by UT-Battelle, Alliance for Sustainable Energy, LLC, and other
// contributors. All rights reserved.
//
// NOTICE: This Software was developed under funding from the U.S. Department of Energy and the
// U.S. Government consequently retains certain rights. As such, the U.S. Government has been
// granted for itself and others acting on its behalf a paid-up, nonexclusive, irrevocable,
// worldwide license in the Software to reproduce, distribute copies to the public, prepare
// derivative works, and perform publicly and display publicly, and to permit others to do so.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
// (1) Redistributions of source code must retain the above copyright notice, this list of
//     conditions and the following disclaimer.
//
// (2) Redistributions in binary form must reproduce the above copyright notice, this list of
//     conditions and the following disclaimer in the documentation and/or other materials
//     provided with the distribution.
//
// (3) Neither the name of the University of California, Lawrence Berkeley National Laboratory,
//     the University of Illinois, U.S. Dept. of Energy nor the names of its contributors may be
//     used to endorse or promote products derived from this software without specific prior
//     written permission.
//
// (4) Use of EnergyPlus(TM) Name. If Licensee (i) distributes the software in stand-alone form
//     without changes from the version obtained under this License, or (ii) Licensee makes a
//     reference solely to the software portion of its product, Licensee must refer to the
//     software as "EnergyPlus version X" software, where "X" is the version number Licensee
//     obtained under this License and may not use a different name for the software. Except as
//     specifically required in this Section (4), Licensee shall not use in a company name, a
//     product name, in advertising, publicity, or other promotional activities any name, trade
//     name, trademark, logo, or other designation of "EnergyPlus", "E+", "e+" or confusingly
//     similar designation, without the U.S. Department of Energy's prior written consent.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// C++ Headers
#include <cassert>
#include <cmath>

// ObjexxFCL Headers
#include <ObjexxFCL/Fmath.hh>

// EnergyPlus Headers
#include <EnergyPlus/BranchNodeConnections.hh>
#include <EnergyPlus/CurveManager.hh>
#include <EnergyPlus/DataBranchAirLoopPlant.hh>
#include <EnergyPlus/DataEnvironment.hh>
#include <EnergyPlus/DataHVACGlobals.hh>
#include <EnergyPlus/DataIPShortCuts.hh>
#include <EnergyPlus/DataLoopNode.hh>
#include <EnergyPlus/Plant/DataPlant.hh>
#include <EnergyPlus/FluidProperties.hh>
#include <EnergyPlus/General.hh>
#include <EnergyPlus/IceThermalStorage.hh>
#include <EnergyPlus/InputProcessing/InputProcessor.hh>
#include <EnergyPlus/NodeInputManager.hh>
#include <EnergyPlus/OutputProcessor.hh>
#include <EnergyPlus/Plant/PlantLocation.hh>
#include <EnergyPlus/PlantUtilities.hh>
#include <EnergyPlus/Psychrometrics.hh>
#include <EnergyPlus/ScheduleManager.hh>
#include <EnergyPlus/UtilityRoutines.hh>

namespace EnergyPlus {

namespace IceThermalStorage {

    // MODULE INFORMATION:
    //       AUTHOR         Pyeongchan Ihm
    //       DATE WRITTEN   April 2002
    //       MODIFIED       Modified Refined model, added Simple model, by Guo Zhou, Oct 2002
    //                      Remove chiller, make just a storage tank, Michael J. Witte, Sep 2005
    //                      Added detailed ice storage model, Rick Strand, Feb 2006
    //                      B. Griffith, Sept 2010, plant upgrades, fluid properties
    //                      Enhancements to detailed ice storage model, Rick Strand, Aug 2012
    //       RE-ENGINEERED  na

    // PURPOSE OF THIS MODULE:
    // This module simulates the performance of Ice Thermal Storage

    // METHODOLOGY EMPLOYED:
    // Once the PlantLoopManager determines that the Ice Thermal Storage
    // is available to meet a loop cooling demand, it calls SimIceStorage
    // which in turn calls the appropriate Ice Thermal Storage model.

    // REFERENCES: Dion J. King, ASHRAE Transactions v104, pt1, 1998.

    std::string const cIceStorageSimple("ThermalStorage:Ice:Simple");
    std::string const cIceStorageDetailed("ThermalStorage:Ice:Detailed");

    // ITS parameter
    Real64 const FreezTemp(0.0);       // Water freezing Temperature, 0[C]
    Real64 const FreezTempIP(32.0);    // Water freezing Temperature, 32[F]
    Real64 const TimeInterval(3600.0); // Time Interval (1 hr) [s]

    // Conversion parameter
    Real64 const EpsLimitForX(0.0);         // 0.02  ! See Dion's code as eps1
    Real64 const EpsLimitForDisCharge(0.0); // 0.20  ! See Dion's code as eps2
    Real64 const EpsLimitForCharge(0.0);    // 0.20  ! See Dion's code as eps3

    // Parameter used by the Detailed Ice Storage Model
    Real64 const DeltaTofMin(0.5); // Minimum allowed outlet side temperature difference [C]
    // This is (Tout - Tfreezing)
    Real64 const DeltaTifMin(1.0); // Minimum allowed inlet side temperature difference [C]
    // This is (Tin - Tfreezing)

    int NumSimpleIceStorage(0);
    int NumDetailedIceStorage(0);
    int TotalNumIceStorage(0);

    bool getITSInput = true;

    // Object Data
    Array1D<SimpleIceStorageData> SimpleIceStorage;     // dimension to number of machines
    Array1D<DetailedIceStorageData> DetailedIceStorage; // Derived type for detailed ice storage model

    //*************************************************************************

    // Functions
    void clear_state()
    {
        getITSInput = true;
        NumSimpleIceStorage = 0;
        NumDetailedIceStorage = 0;
        TotalNumIceStorage = 0;
        SimpleIceStorage.deallocate();
        DetailedIceStorage.deallocate();
    }

    PlantComponent *SimpleIceStorageData::factory(std::string const &objectName)
    {
        // Process the input data for boilers if it hasn't been done already
        if (getITSInput) {
            GetIceStorageInput();
            getITSInput = false;
        }

        // Now look for this particular pipe in the list
        for (auto &ITS : SimpleIceStorage) {
            if (ITS.Name == objectName) {
                return &ITS;
            }
        }
        // If we didn't find it, fatal
        ShowFatalError("LocalSimpleIceStorageFactory: Error getting inputs for simple ice storage named: " + objectName); // LCOV_EXCL_LINE
        // Shut up the compiler
        return nullptr; // LCOV_EXCL_LINE
    }

    PlantComponent *DetailedIceStorageData::factory(std::string const &objectName)
    {
        // Process the input data for boilers if it hasn't been done already
        if (getITSInput) {
            GetIceStorageInput();
            getITSInput = false;
        }

        // Now look for this particular pipe in the list
        for (auto &ITS : DetailedIceStorage) {
            if (ITS.Name == objectName) {
                return &ITS;
            }
        }
        // If we didn't find it, fatal
        ShowFatalError("LocalDetailedIceStorageFactory: Error getting inputs for detailed ice storage named: " + objectName); // LCOV_EXCL_LINE
        // Shut up the compiler
        return nullptr; // LCOV_EXCL_LINE
    }

    void SimpleIceStorageData::simulate(const PlantLocation &calledFromLocation,
                                        bool EP_UNUSED(FirstHVACIteration),
                                        Real64 &EP_UNUSED(CurLoad),
                                        bool RunFlag)
    {
        static constexpr std::string_view RoutineName("SimpleIceStorageData::simulate");

        // this was happening in PlantLoopEquip before
        auto &thisComp(DataPlant::PlantLoop(calledFromLocation.loopNum).LoopSide(calledFromLocation.loopSideNum).Branch(calledFromLocation.branchNum).Comp(calledFromLocation.compNum));

        // If component setpoint based control is active for this equipment
        // then reset CurLoad to original EquipDemand.
        // Allow negative CurLoad.  For cold storage this means the storage should
        // charge, for hot storage, this means the storage should discharge.
        if (thisComp.CurOpSchemeType == DataPlant::CompSetPtBasedSchemeType) {
            Real64 localCurLoad = thisComp.EquipDemand;
            if (localCurLoad != 0) RunFlag = true;
        }

        if (DataGlobals::BeginEnvrnFlag && this->MyEnvrnFlag) {
            this->ResetXForITSFlag = true;
            this->MyEnvrnFlag = false;
        }

        if (!DataGlobals::BeginEnvrnFlag) {
            this->MyEnvrnFlag = true;
        }

        this->InitSimpleIceStorage();

        //------------------------------------------------------------------------
        // FIRST PROCESS (MyLoad = 0.0 as IN)
        // At this moment as first calling of ITS, ITS provide ONLY MaxCap/OptCap/MinCap.
        //------------------------------------------------------------------------
        // First process is in subroutine CalcIceStorageCapacity(MaxCap,MinCap,OptCap) shown bellow.

        //------------------------------------------------------------------------
        // SECOND PROCESS (MyLoad is provided by E+ based on MaxCap/OptCap/MinCap)
        //------------------------------------------------------------------------
        // Below routines are starting when second calling.
        // After previous return, MyLoad is calculated based on MaxCap, OptCap, and MinCap.
        // Then PlandSupplySideManager provides MyLoad to simulate Ice Thermal Storage.
        // The process will be decided based on sign(+,-,0) of input U.

        // MJW 19 Sep 2005 - New approach - calculate MyLoad locally from inlet node temp
        //                   and outlet node setpoint until MyLoad that is passed in behaves well

        // DSU? can we now use MyLoad? lets not yet to try to avoid scope creep

        Real64 TempSetPt(0.0);
        Real64 TempIn = DataLoopNode::Node(this->PltInletNodeNum).Temp;
        {
            auto const SELECT_CASE_var1(DataPlant::PlantLoop(this->LoopNum).LoopDemandCalcScheme);
            if (SELECT_CASE_var1 == DataPlant::SingleSetPoint) {
                TempSetPt = DataLoopNode::Node(this->PltOutletNodeNum).TempSetPoint;
            } else if (SELECT_CASE_var1 == DataPlant::DualSetPointDeadBand) {
                TempSetPt = DataLoopNode::Node(this->PltOutletNodeNum).TempSetPointHi;
            } else {
                assert(false);
            }
        }
        Real64 DemandMdot = this->DesignMassFlowRate;

        Real64 Cp = FluidProperties::GetSpecificHeatGlycol(
            DataPlant::PlantLoop(this->LoopNum).FluidName, TempIn, DataPlant::PlantLoop(this->LoopNum).FluidIndex, RoutineName);

        Real64 MyLoad2 = (DemandMdot * Cp * (TempIn - TempSetPt));
        MyLoad = MyLoad2;

        //     Set fraction of ice remaining in storage
        this->XCurIceFrac = this->IceFracRemain;

        //***** Dormant Process for ITS *****************************************
        //************************************************************************
        //        IF( U .EQ. 0.0 ) THEN
        if ((MyLoad2 == 0.0) || (DemandMdot == 0.0)) {
            this->CalcIceStorageDormant();

            //***** Charging Process for ITS *****************************************
            //************************************************************************
            //        ELSE IF( U .GT. 0.0 ) THEN
        } else if (MyLoad2 < 0.0) {

            Real64 MaxCap;
            Real64 MinCap;
            Real64 OptCap;
            this->CalcIceStorageCapacity(MaxCap, MinCap, OptCap);
            this->CalcIceStorageCharge();

            //***** Discharging Process for ITS *****************************************
            //************************************************************************
            //        ELSE IF( U .LT. 0.0 ) THEN
        } else if (MyLoad2 > 0.0) {

            Real64 MaxCap;
            Real64 MinCap;
            Real64 OptCap;
            this->CalcIceStorageCapacity(MaxCap, MinCap, OptCap);
            this->CalcIceStorageDischarge(MyLoad, RunFlag, MaxCap);
        } // Based on input of U value, deciding Dormant/Charge/Discharge process

        // Update Node properties: mdot and Temperature
        this->UpdateNode(MyLoad2, RunFlag);

        // Update report variables.
        this->RecordOutput(MyLoad2, RunFlag);
    }

    void DetailedIceStorageData::simulate(const PlantLocation &EP_UNUSED(calledFromLocation),
                                          bool EP_UNUSED(FirstHVACIteration),
                                          Real64 &EP_UNUSED(CurLoad),
                                          bool EP_UNUSED(RunFlag))
    {

        if (DataGlobals::BeginEnvrnFlag && this->MyEnvrnFlag) {
            this->ResetXForITSFlag = true;
            this->MyEnvrnFlag = false;
        }

        if (!DataGlobals::BeginEnvrnFlag) {
            this->MyEnvrnFlag = true;
        }

        this->InitDetailedIceStorage(); // Initialize detailed ice storage

        this->SimDetailedIceStorage(); // Simulate detailed ice storage

        this->UpdateDetailedIceStorage(); // Update detailed ice storage

        this->ReportDetailedIceStorage(); // Report detailed ice storage
    }

    void DetailedIceStorageData::SimDetailedIceStorage()
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Rick Strand
        //       DATE WRITTEN   February 2006
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is the main simulation subroutine for the detailed
        // ice storage model.

        // METHODOLOGY EMPLOYED:
        // Based on whether the unit is dormant, in charging mode, or in discharging
        // mode, the code either passes the flow through the bypass, through the tank,
        // or both.  This depends on the temperature relative to the setpoint temperature
        // and other features of the model.  The model itself is a LMTD model that uses
        // performance curve fits that are quadratic in fraction charged/discharged and
        // linear in LMTD for the calculation of Q.  The equations are actually non-
        // dimensionalized.

        // REFERENCES:
        // Ice Storage Component Model Proposal (Revised).doc by Rick Strand (Dec 2005/Jan 2006)

        int const MaxIterNum(100);                      // Maximum number of internal iterations for ice storage solution
        Real64 const SmallestLoad(0.1);                 // Smallest load to actually run the ice storage unit [Watts]
        Real64 const TankDischargeToler(0.001);         // Below this fraction, there is nothing left to discharge
        Real64 const TankChargeToler(0.999);            // Above this fraction, we don't have anything left to charge
        Real64 const TemperatureToler(0.1);             // Temperature difference between iterations that indicates convergence [C]
        Real64 const SIEquiv100GPMinMassFlowRate(6.31); // Used to non-dimensionalize flow rate for use in CubicLinear charging equation
                                                        // Flow rate divided by nominal 100GPM used to non-dimensionalize volume flow rate
                                                        // Assumes approximate density of 1000 kg/m3 to get an estimate for mass flow rate
        static constexpr std::string_view RoutineName("DetailedIceStorageData::SimDetailedIceStorage");

        int NodeNumIn = this->PlantInNodeNum;               // Plant loop inlet node number for component
        int NodeNumOut = this->PlantOutNodeNum;             // Plant loop outlet node number for component
        Real64 TempIn = DataLoopNode::Node(NodeNumIn).Temp; // Inlet temperature to component (from plant loop) [C]
        Real64 TempSetPt(0.0);                              // Setpoint temperature defined by loop controls [C]
        {
            auto const SELECT_CASE_var(DataPlant::PlantLoop(this->PlantLoopNum).LoopDemandCalcScheme);
            if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                TempSetPt = DataLoopNode::Node(NodeNumOut).TempSetPoint;
            } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                TempSetPt = DataLoopNode::Node(NodeNumOut).TempSetPointHi;
            } else {
                assert(false);
            }
        }

        int IterNum = 0;

        // Set derived type variables
        this->InletTemp = TempIn;
        this->MassFlowRate = DataLoopNode::Node(NodeNumIn).MassFlowRate;

        // if two-way common pipe and no mass flow and tank is not full, then use design flow rate
        if ((DataPlant::PlantLoop(this->PlantLoopNum).CommonPipeType == DataPlant::CommonPipe_TwoWay) &&
            (std::abs(this->MassFlowRate) < DataBranchAirLoopPlant::MassFlowTolerance) && (this->IceFracRemaining < TankChargeToler)) {
            this->MassFlowRate = this->DesignMassFlowRate;
        }

        // Calculate the current load on the ice storage unit
        Real64 Cp = FluidProperties::GetSpecificHeatGlycol(
            DataPlant::PlantLoop(this->PlantLoopNum).FluidName, TempIn, DataPlant::PlantLoop(this->PlantLoopNum).FluidIndex, RoutineName);

        // Estimated load on the ice storage unit [W]
        Real64 LocalLoad = this->MassFlowRate * Cp * (TempIn - TempSetPt);

        // Determine what the status is regarding the ice storage unit and the loop level flow
        if ((std::abs(LocalLoad) <= SmallestLoad) || (ScheduleManager::GetCurrentScheduleValue(this->ScheduleIndex) <= 0)) {
            // No real load on the ice storage device or ice storage OFF--bypass all of the flow and leave the tank alone
            this->CompLoad = 0.0;
            this->OutletTemp = TempIn;
            this->TankOutletTemp = TempIn;
            Real64 mdot = 0.0;
            PlantUtilities::SetComponentFlowRate(mdot,
                                                 this->PlantInNodeNum,
                                                 this->PlantOutNodeNum,
                                                 this->PlantLoopNum,
                                                 this->PlantLoopSideNum,
                                                 this->PlantBranchNum,
                                                 this->PlantCompNum);

            this->BypassMassFlowRate = mdot;
            this->TankMassFlowRate = 0.0;
            this->MassFlowRate = mdot;

        } else if (LocalLoad < 0.0) {
            // The load is less than zero so we should be charging
            // Before we do anything, we should check to make sure that we will actually be charging the unit

            if ((TempIn > (this->FreezingTemp - DeltaTifMin)) || (this->IceFracRemaining >= TankChargeToler)) {
                // If the inlet temperature is not below the freezing temperature of the
                // device, then we cannot actually do any charging.  Bypass all of the flow.
                // Also, if the tank is already sufficiently charged, we don't need to
                // do any further charging.  So, bypass all of the flow.
                this->CompLoad = 0.0;
                this->OutletTemp = TempIn;
                this->TankOutletTemp = TempIn;
                Real64 mdot = 0.0;
                PlantUtilities::SetComponentFlowRate(mdot,
                                                     this->PlantInNodeNum,
                                                     this->PlantOutNodeNum,
                                                     this->PlantLoopNum,
                                                     this->PlantLoopSideNum,
                                                     this->PlantBranchNum,
                                                     this->PlantCompNum);

                this->BypassMassFlowRate = mdot;
                this->TankMassFlowRate = 0.0;
                this->MassFlowRate = mdot;

            } else {
                // make flow request so tank will get flow
                Real64 mdot = this->DesignMassFlowRate;
                PlantUtilities::SetComponentFlowRate(mdot,
                                                     this->PlantInNodeNum,
                                                     this->PlantOutNodeNum,
                                                     this->PlantLoopNum,
                                                     this->PlantLoopSideNum,
                                                     this->PlantBranchNum,
                                                     this->PlantCompNum);

                // We are in charging mode, the temperatures are low enough to charge
                // the tank, and we have some charging left to do.
                // Make first guess at Qstar based on the current ice fraction remaining
                // and LMTDstar that is based on the freezing or TempSetPt temperature.
                if (TempSetPt > (this->FreezingTemp - DeltaTofMin)) {
                    // Outlet temperature cannot be above the freezing temperature so set
                    // the outlet temperature to the freezing temperature and calculate
                    // LMTDstar based on that assumption.
                    TempSetPt = this->FreezingTemp - DeltaTofMin;
                }

                // Tank outlet temperature from the last iteration [C]
                Real64 ToutOld = TempSetPt;
                // Non-dimensional log mean temperature difference of ice storage unit [non-dimensional]
                Real64 LMTDstar = CalcDetIceStorLMTDstar(TempIn, ToutOld, this->FreezingTemp);
                Real64 MassFlowstar = this->MassFlowRate / SIEquiv100GPMinMassFlowRate;

                // Find initial guess at average fraction charged during time step
                // Fraction of tank to be charged in the current time step
                Real64 ChargeFrac = LocalLoad * DataHVACGlobals::TimeStepSys / this->NomCapacity;
                if ((this->IceFracRemaining + ChargeFrac) > 1.0) {
                    ChargeFrac = 1.0 - this->IceFracRemaining;
                }

                Real64 AvgFracCharged; // Average fraction charged for the current time step
                if (this->ThawProcessIndex == DetIce::InsideMelt) {
                    AvgFracCharged = this->IceFracOnCoil + (ChargeFrac / 2.0);
                } else { // (DetailedIceStorage(IceNum)%ThawProcessIndex == DetIce::OutsideMelt)
                    AvgFracCharged = this->IceFracRemaining + (ChargeFrac / 2.0);
                }

                // Current load on the ice storage unit [non-dimensional]
                Real64 Qstar = std::abs(CalcQstar(this->ChargeCurveNum, this->ChargeCurveTypeNum, AvgFracCharged, LMTDstar, MassFlowstar));

                // Actual load on the ice storage unit [W]
                Real64 ActualLoad = Qstar * this->NomCapacity / this->CurveFitTimeStep;

                // Updated outlet temperature from the tank [C]
                Real64 ToutNew = TempIn + (ActualLoad / (this->MassFlowRate * Cp));
                // Again, the outlet temperature cannot be above the freezing temperature (factoring in the tolerance)
                if (ToutNew > (this->FreezingTemp - DeltaTofMin)) ToutNew = this->FreezingTemp - DeltaTofMin;

                if (ActualLoad > std::abs(LocalLoad)) {
                    // We have more than enough capacity to meet the load so no need to iterate to find a solution
                    this->OutletTemp = TempSetPt;
                    this->TankOutletTemp = ToutNew;
                    this->CompLoad = this->MassFlowRate * Cp * std::abs(TempIn - TempSetPt);
                    this->TankMassFlowRate = this->CompLoad / Cp / std::abs(TempIn - ToutNew);
                    this->BypassMassFlowRate = this->MassFlowRate - this->TankMassFlowRate;

                } else {

                    while (IterNum < MaxIterNum) {
                        if (std::abs(ToutOld - ToutNew) > TemperatureToler) {
                            // Not converged yet so recalculated what is needed and keep iterating
                            // Calculate new values for LMTDstar and Qstar based on updated outlet temperature
                            ToutOld = ToutNew;
                            LMTDstar = CalcDetIceStorLMTDstar(TempIn, ToutOld, this->FreezingTemp);
                            MassFlowstar = this->MassFlowRate / SIEquiv100GPMinMassFlowRate;
                            Qstar = std::abs(CalcQstar(this->ChargeCurveNum, this->ChargeCurveTypeNum, AvgFracCharged, LMTDstar, MassFlowstar));

                            // Now make sure that we don't go above 100% charged and calculate the new average fraction
                            ChargeFrac = Qstar * (DataHVACGlobals::TimeStepSys / this->CurveFitTimeStep);
                            if ((this->IceFracRemaining + ChargeFrac) > 1.0) {
                                ChargeFrac = 1.0 - this->IceFracRemaining;
                                Qstar = ChargeFrac;
                            }
                            if (this->ThawProcessIndex == DetIce::InsideMelt) {
                                AvgFracCharged = this->IceFracOnCoil + (ChargeFrac / 2.0);
                            } else { // (DetailedIceStorage(IceNum)%ThawProcessIndex == DetIce::OutsideMelt)
                                AvgFracCharged = this->IceFracRemaining + (ChargeFrac / 2.0);
                            }

                            // Finally, update the actual load and calculate the new outlet temperature; increment iteration counter
                            ActualLoad = Qstar * this->NomCapacity / this->CurveFitTimeStep;
                            ToutNew = TempIn + (ActualLoad / (this->MassFlowRate * Cp));
                            // Again, the outlet temperature cannot be above the freezing temperature (factoring in the tolerance)
                            if (ToutNew < (this->FreezingTemp - DeltaTofMin)) ToutNew = this->FreezingTemp - DeltaTofMin;
                            ++IterNum;

                        } else {
                            // Converged to acceptable tolerance so set output variables and exit DO WHILE loop
                            break;
                        }

                    } // ...loop iterating for the ice storage outlet temperature

                    // Keep track of times that the iterations got excessive and report if necessary
                    if (IterNum >= MaxIterNum) {
                        ++this->ChargeIterErrors;
                        if (this->ChargeIterErrors <= 25) {
                            ShowWarningError("Detailed Ice Storage model exceeded its internal charging maximum iteration limit");
                            ShowContinueError("Detailed Ice Storage System Name = " + this->Name);
                            ShowContinueErrorTimeStamp("");
                        } else {
                            ShowRecurringWarningErrorAtEnd("Detailed Ice Storage system [" + this->Name +
                                                               "]  charging maximum iteration limit exceeded occurrence continues.",
                                                           this->ChargeErrorCount);
                        }
                    }

                    // Set the values for the key outlet parameters
                    // Note that in REAL(r64)ity the tank will probably bypass some flow when it
                    // gets close to full charge.  This is a simplification that assumes
                    // all flow through the tank during charging and a lower delta T near
                    // the full charge level.  From an energy perspective, this is a reasonable
                    // approximation.
                    this->OutletTemp = ToutNew;
                    this->TankOutletTemp = ToutNew;
                    this->BypassMassFlowRate = 0.0;
                    this->TankMassFlowRate = this->MassFlowRate;
                    this->CompLoad = this->MassFlowRate * Cp * std::abs(TempIn - ToutNew);
                }
            }

        } else if (LocalLoad > 0.0) {
            // The load is greater than zero so we should be discharging
            // Before we do anything, we should check to make sure that we will actually be discharging the unit

            if ((this->InletTemp < (this->FreezingTemp + DeltaTifMin)) || (this->IceFracRemaining <= TankDischargeToler)) {
                // If the inlet temperature is below the freezing temperature of the
                // device, then we cannot actually do any discharging.  Bypass all of the flow.
                // Also, if the tank is already discharged, we can't to do any further
                // discharging.  So, bypass all of the flow.
                this->CompLoad = 0.0;
                this->OutletTemp = this->InletTemp;
                this->TankOutletTemp = this->InletTemp;
                Real64 mdot = 0.0;
                PlantUtilities::SetComponentFlowRate(mdot,
                                                     this->PlantInNodeNum,
                                                     this->PlantOutNodeNum,
                                                     this->PlantLoopNum,
                                                     this->PlantLoopSideNum,
                                                     this->PlantBranchNum,
                                                     this->PlantCompNum);

                this->BypassMassFlowRate = mdot;
                this->TankMassFlowRate = 0.0;
                this->MassFlowRate = mdot;

            } else {

                // make flow request so tank will get flow
                Real64 mdot = this->DesignMassFlowRate;
                PlantUtilities::SetComponentFlowRate(mdot,
                                                     this->PlantInNodeNum,
                                                     this->PlantOutNodeNum,
                                                     this->PlantLoopNum,
                                                     this->PlantLoopSideNum,
                                                     this->PlantBranchNum,
                                                     this->PlantCompNum);

                // We are in discharging mode, the temperatures are high enough to discharge
                // the tank, and we have some discharging left to do.
                if (TempSetPt < (this->FreezingTemp + DeltaTofMin)) {
                    // Outlet temperature cannot be below the freezing temperature so set
                    // the outlet temperature to the freezing temperature and calculate
                    // LMTDstar based on that assumption.
                    TempSetPt = this->FreezingTemp + DeltaTofMin;
                }

                // Tank outlet temperature from the last iteration [C]
                Real64 ToutOld = TempSetPt;
                // Non-dimensional log mean temperature difference of ice storage unit [non-dimensional]
                Real64 LMTDstar = CalcDetIceStorLMTDstar(TempIn, ToutOld, this->FreezingTemp);
                Real64 MassFlowstar = this->MassFlowRate / SIEquiv100GPMinMassFlowRate;

                // Find initial guess at average fraction charged during time step
                Real64 ChargeFrac = LocalLoad * DataHVACGlobals::TimeStepSys / this->NomCapacity;
                if ((this->IceFracRemaining - ChargeFrac) < 0.0) ChargeFrac = this->IceFracRemaining;
                Real64 AvgFracCharged = this->IceFracRemaining - (ChargeFrac / 2.0);

                // Current load on the ice storage unit [non-dimensional]
                Real64 Qstar = std::abs(CalcQstar(this->DischargeCurveNum, this->DischargeCurveTypeNum, AvgFracCharged, LMTDstar, MassFlowstar));

                // Actual load on the ice storage unit [W]
                Real64 ActualLoad = Qstar * this->NomCapacity / this->CurveFitTimeStep;

                // Updated outlet temperature from the tank [C]
                Real64 ToutNew = TempIn - (ActualLoad / (this->MassFlowRate * Cp));
                // Again, the outlet temperature cannot be below the freezing temperature (factoring in the tolerance)
                if (ToutNew < (this->FreezingTemp + DeltaTofMin)) ToutNew = this->FreezingTemp + DeltaTofMin;

                if (ActualLoad > LocalLoad) {
                    // We have more than enough storage to meet the load so no need to iterate to find a solution
                    this->OutletTemp = TempSetPt;
                    this->TankOutletTemp = ToutNew;
                    this->CompLoad = this->MassFlowRate * Cp * std::abs(TempIn - TempSetPt);
                    this->TankMassFlowRate = this->CompLoad / Cp / std::abs(TempIn - ToutNew);
                    this->BypassMassFlowRate = this->MassFlowRate - this->TankMassFlowRate;

                } else {

                    while (IterNum < MaxIterNum) {
                        if (std::abs(ToutOld - ToutNew) > TemperatureToler) {
                            // Not converged yet so recalculated what is needed and keep iterating
                            // Calculate new values for LMTDstar and Qstar based on updated outlet temperature
                            ToutOld = ToutNew;
                            LMTDstar = CalcDetIceStorLMTDstar(TempIn, ToutOld, this->FreezingTemp);

                            Qstar = std::abs(CalcQstar(this->DischargeCurveNum, this->DischargeCurveTypeNum, AvgFracCharged, LMTDstar, MassFlowstar));

                            // Now make sure that we don't go below 100% discharged and calculate the new average fraction
                            ChargeFrac = Qstar * (DataHVACGlobals::TimeStepSys / this->CurveFitTimeStep);
                            if ((this->IceFracRemaining - ChargeFrac) < 0.0) {
                                ChargeFrac = this->IceFracRemaining;
                                Qstar = ChargeFrac;
                            }
                            AvgFracCharged = this->IceFracRemaining - (ChargeFrac / 2.0);

                            // Finally, update the actual load and calculate the new outlet temperature; increment iteration counter
                            ActualLoad = Qstar * this->NomCapacity / this->CurveFitTimeStep;
                            ToutNew = TempIn - (ActualLoad / (this->MassFlowRate * Cp));
                            // Again, the outlet temperature cannot be below the freezing temperature (factoring in the tolerance)
                            if (ToutNew < (this->FreezingTemp + DeltaTofMin)) ToutNew = this->FreezingTemp + DeltaTofMin;
                            ++IterNum;

                        } else {
                            // Converged to acceptable tolerance so set output variables and exit DO WHILE loop
                            break;
                        }

                    } // ...loop iterating for the ice storage outlet temperature

                    // Keep track of times that the iterations got excessive
                    if (IterNum >= MaxIterNum && (!DataGlobals::WarmupFlag)) {
                        ++this->DischargeIterErrors;
                        if (this->DischargeIterErrors <= 25) {
                            ShowWarningError("Detailed Ice Storage model exceeded its internal discharging maximum iteration limit");
                            ShowContinueError("Detailed Ice Storage System Name = " + this->Name);
                            ShowContinueErrorTimeStamp("");
                        } else {
                            ShowRecurringWarningErrorAtEnd("Detailed Ice Storage system [" + this->Name +
                                                               "]  discharging maximum iteration limit exceeded occurrence continues.",
                                                           this->DischargeErrorCount);
                        }
                    }

                    // We are now done finding the outlet temperature of the tank.  We need
                    // to compare the outlet temperature to the setpoint temperature again
                    // to see where we are at and then we can set the values for the key
                    // outlet parameters.  If outlet temperature is greater than or equal
                    // to the setpoint temperature, then send all flow through the tank.
                    // Otherwise, we have more capacity than needed so let's bypass some
                    // flow and meet the setpoint temperautre.
                    if (ToutNew >= TempSetPt) {
                        this->OutletTemp = ToutNew;
                        this->TankOutletTemp = ToutNew;
                        this->BypassMassFlowRate = 0.0;
                        this->TankMassFlowRate = this->MassFlowRate;
                        this->CompLoad = this->MassFlowRate * Cp * std::abs(TempIn - ToutNew);
                    } else {
                        this->OutletTemp = TempSetPt;
                        this->TankOutletTemp = ToutNew;
                        this->CompLoad = this->MassFlowRate * Cp * std::abs(TempIn - TempSetPt);
                        this->TankMassFlowRate = this->CompLoad / (Cp * std::abs(TempIn - ToutNew));
                        this->BypassMassFlowRate = this->MassFlowRate - this->TankMassFlowRate;
                    }
                }
            }

        } else { // Shouldn't get here ever (print error if we do)

            ShowFatalError("Detailed Ice Storage systemic code error--contact EnergyPlus support");
        }
    }

    void GetIceStorageInput()
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR:
        //       DATE WRITTEN:

        // PURPOSE OF THIS SUBROUTINE:!This routine will get the input
        // required by the PrimaryPlantLoopManager.  As such
        // it will interact with the Input Scanner to retrieve
        // information from the input file, count the number of
        // heating and cooling loops and begin to fill the
        // arrays associated with the type PlantLoopProps.

        bool ErrorsFound;

        ErrorsFound = false; // Always need to reset this since there are multiple types of ice storage systems

        // LOAD ARRAYS WITH SimpleIceStorage DATA
        NumSimpleIceStorage = inputProcessor->getNumObjectsFound(cIceStorageSimple); // by ZG
        NumDetailedIceStorage = inputProcessor->getNumObjectsFound(cIceStorageDetailed);

        // Allocate SimpleIceStorage based on NumOfIceStorage
        SimpleIceStorage.allocate(NumSimpleIceStorage);

        DataIPShortCuts::cCurrentModuleObject = cIceStorageSimple;
        for (int iceNum = 1; iceNum <= NumSimpleIceStorage; ++iceNum) {

            int NumAlphas;
            int NumNums;
            int IOStat;
            inputProcessor->getObjectItem(DataIPShortCuts::cCurrentModuleObject,
                                          iceNum,
                                          DataIPShortCuts::cAlphaArgs,
                                          NumAlphas,
                                          DataIPShortCuts::rNumericArgs,
                                          NumNums,
                                          IOStat,
                                          _,
                                          _,
                                          _,
                                          DataIPShortCuts::cNumericFieldNames);
            UtilityRoutines::IsNameEmpty(DataIPShortCuts::cAlphaArgs(1), DataIPShortCuts::cCurrentModuleObject, ErrorsFound);

            ++TotalNumIceStorage;
            SimpleIceStorage(iceNum).MapNum = TotalNumIceStorage;

            // ITS name
            SimpleIceStorage(iceNum).Name = DataIPShortCuts::cAlphaArgs(1);

            // Get Ice Thermal Storage Type
            SimpleIceStorage(iceNum).ITSType = DataIPShortCuts::cAlphaArgs(2);
            if (UtilityRoutines::SameString(SimpleIceStorage(iceNum).ITSType, "IceOnCoilInternal")) {
                SimpleIceStorage(iceNum).ITSType_Num = ITSType::IceOnCoilInternal;
            } else if (UtilityRoutines::SameString(SimpleIceStorage(iceNum).ITSType, "IceOnCoilExternal")) {
                SimpleIceStorage(iceNum).ITSType_Num = ITSType::IceOnCoilExternal;
            } else {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Invalid " + DataIPShortCuts::cAlphaFieldNames(2) + '=' + DataIPShortCuts::cAlphaArgs(2));
                ErrorsFound = true;
            }

            // Get and Verify ITS nominal Capacity (user input is in GJ, internal value in in J)
            SimpleIceStorage(iceNum).ITSNomCap = DataIPShortCuts::rNumericArgs(1) * 1.e+09;
            if (DataIPShortCuts::rNumericArgs(1) == 0.0) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Invalid " + DataIPShortCuts::cNumericFieldNames(1) + '=' +
                                  General::RoundSigDigits(DataIPShortCuts::rNumericArgs(1), 2));
                ErrorsFound = true;
            }

            // Get Plant Inlet Node Num
            SimpleIceStorage(iceNum).PltInletNodeNum = NodeInputManager::GetOnlySingleNode(DataIPShortCuts::cAlphaArgs(3),
                                                                                           ErrorsFound,
                                                                                           DataIPShortCuts::cCurrentModuleObject,
                                                                                           DataIPShortCuts::cAlphaArgs(1),
                                                                                           DataLoopNode::NodeType_Water,
                                                                                           DataLoopNode::NodeConnectionType_Inlet,
                                                                                           1,
                                                                                           DataLoopNode::ObjectIsNotParent);

            // Get Plant Outlet Node Num
            SimpleIceStorage(iceNum).PltOutletNodeNum = NodeInputManager::GetOnlySingleNode(DataIPShortCuts::cAlphaArgs(4),
                                                                                            ErrorsFound,
                                                                                            DataIPShortCuts::cCurrentModuleObject,
                                                                                            DataIPShortCuts::cAlphaArgs(1),
                                                                                            DataLoopNode::NodeType_Water,
                                                                                            DataLoopNode::NodeConnectionType_Outlet,
                                                                                            1,
                                                                                            DataLoopNode::ObjectIsNotParent);

            // Test InletNode and OutletNode
            BranchNodeConnections::TestCompSet(DataIPShortCuts::cCurrentModuleObject,
                                               DataIPShortCuts::cAlphaArgs(1),
                                               DataIPShortCuts::cAlphaArgs(3),
                                               DataIPShortCuts::cAlphaArgs(4),
                                               "Chilled Water Nodes");

            // Initialize Report Variables
            SimpleIceStorage(iceNum).MyLoad = 0.0;
            SimpleIceStorage(iceNum).Urate = 0.0;
            SimpleIceStorage(iceNum).IceFracRemain = 1.0;
            SimpleIceStorage(iceNum).ITSCoolingRate_rep = 0.0;
            SimpleIceStorage(iceNum).ITSCoolingEnergy_rep = 0.0;
            SimpleIceStorage(iceNum).ITSChargingRate = 0.0;
            SimpleIceStorage(iceNum).ITSChargingEnergy = 0.0;
            SimpleIceStorage(iceNum).ITSmdot = 0.0;
            SimpleIceStorage(iceNum).ITSInletTemp = 0.0;
            SimpleIceStorage(iceNum).ITSOutletTemp = 0.0;

        } // IceNum

        if (ErrorsFound) {
            ShowFatalError("Errors found in processing input for " + DataIPShortCuts::cCurrentModuleObject);
        }

        ErrorsFound = false; // Always need to reset this since there are multiple types of ice storage systems

        // Determine the number of detailed ice storage devices are in the input file and allocate appropriately
        DataIPShortCuts::cCurrentModuleObject = cIceStorageDetailed;

        DetailedIceStorage.allocate(NumDetailedIceStorage); // Allocate DetIceStorage based on NumDetIceStorages

        for (int iceNum = 1; iceNum <= NumDetailedIceStorage; ++iceNum) {

            int NumAlphas;
            int NumNums;
            int IOStat;
            inputProcessor->getObjectItem(DataIPShortCuts::cCurrentModuleObject,
                                          iceNum,
                                          DataIPShortCuts::cAlphaArgs,
                                          NumAlphas,
                                          DataIPShortCuts::rNumericArgs,
                                          NumNums,
                                          IOStat,
                                          _,
                                          DataIPShortCuts::lAlphaFieldBlanks,
                                          DataIPShortCuts::cAlphaFieldNames,
                                          DataIPShortCuts::cNumericFieldNames);
            UtilityRoutines::IsNameEmpty(DataIPShortCuts::cAlphaArgs(1), DataIPShortCuts::cCurrentModuleObject, ErrorsFound);

            ++TotalNumIceStorage;

            DetailedIceStorage(iceNum).MapNum = TotalNumIceStorage;
            DetailedIceStorage(iceNum).Name = DataIPShortCuts::cAlphaArgs(1); // Detailed ice storage name

            // Get and verify availability schedule
            DetailedIceStorage(iceNum).ScheduleName = DataIPShortCuts::cAlphaArgs(2); // Detailed ice storage availability schedule name
            if (DataIPShortCuts::lAlphaFieldBlanks(2)) {
                DetailedIceStorage(iceNum).ScheduleIndex = DataGlobals::ScheduleAlwaysOn;
            } else {
                DetailedIceStorage(iceNum).ScheduleIndex = ScheduleManager::GetScheduleIndex(DetailedIceStorage(iceNum).ScheduleName);
                if (DetailedIceStorage(iceNum).ScheduleIndex == 0) {
                    ShowSevereError("Invalid " + DataIPShortCuts::cAlphaFieldNames(2) + '=' + DataIPShortCuts::cAlphaArgs(2));
                    ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                    ErrorsFound = true;
                }
            }

            // Get and Verify ITS nominal Capacity (user input is in GJ, internal value is in W-hr)
            // Convert GJ to J by multiplying by 10^9
            // Convert J to W-hr by dividing by number of seconds in an hour (3600)
            DetailedIceStorage(iceNum).NomCapacity = DataIPShortCuts::rNumericArgs(1) * (1.e+09) / (DataGlobals::SecInHour);

            if (DataIPShortCuts::rNumericArgs(1) <= 0.0) {
                ShowSevereError("Invalid " + DataIPShortCuts::cNumericFieldNames(1) + '=' +
                                General::RoundSigDigits(DataIPShortCuts::rNumericArgs(1), 2));
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ErrorsFound = true;
            }

            // Get Plant Inlet Node Num
            DetailedIceStorage(iceNum).PlantInNodeNum = NodeInputManager::GetOnlySingleNode(DataIPShortCuts::cAlphaArgs(3),
                                                                                            ErrorsFound,
                                                                                            DataIPShortCuts::cCurrentModuleObject,
                                                                                            DataIPShortCuts::cAlphaArgs(1),
                                                                                            DataLoopNode::NodeType_Water,
                                                                                            DataLoopNode::NodeConnectionType_Inlet,
                                                                                            1,
                                                                                            DataLoopNode::ObjectIsNotParent);

            // Get Plant Outlet Node Num
            DetailedIceStorage(iceNum).PlantOutNodeNum = NodeInputManager::GetOnlySingleNode(DataIPShortCuts::cAlphaArgs(4),
                                                                                             ErrorsFound,
                                                                                             DataIPShortCuts::cCurrentModuleObject,
                                                                                             DataIPShortCuts::cAlphaArgs(1),
                                                                                             DataLoopNode::NodeType_Water,
                                                                                             DataLoopNode::NodeConnectionType_Outlet,
                                                                                             1,
                                                                                             DataLoopNode::ObjectIsNotParent);

            // Test InletNode and OutletNode
            BranchNodeConnections::TestCompSet(DataIPShortCuts::cCurrentModuleObject,
                                               DataIPShortCuts::cAlphaArgs(1),
                                               DataIPShortCuts::cAlphaArgs(3),
                                               DataIPShortCuts::cAlphaArgs(4),
                                               "Chilled Water Nodes");

            // Obtain the Charging and Discharging Curve types and names
            DetailedIceStorage(iceNum).DischargeCurveName = DataIPShortCuts::cAlphaArgs(6);
            DetailedIceStorage(iceNum).DischargeCurveNum = CurveManager::GetCurveIndex(DataIPShortCuts::cAlphaArgs(6));
            if (DetailedIceStorage(iceNum).DischargeCurveNum <= 0) {
                ShowSevereError("Invalid " + DataIPShortCuts::cAlphaFieldNames(6) + '=' + DataIPShortCuts::cAlphaArgs(6));
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ErrorsFound = true;
            }

            int dischargeCurveDim = CurveManager::PerfCurve(DetailedIceStorage(iceNum).DischargeCurveNum).NumDims;
            if (dischargeCurveDim != 2) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + ": Discharge curve must have 2 independent variables");
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError(DataIPShortCuts::cAlphaArgs(6) +
                                  " does not have 2 independent variables and thus cannot be used for detailed ice storage");
                ErrorsFound = true;
            } else {
                if (DataIPShortCuts::cAlphaArgs(5) == "FRACTIONCHARGEDLMTD") {
                    DetailedIceStorage(iceNum).DischargeCurveTypeNum = CurveVars::FracChargedLMTD;
                } else if (DataIPShortCuts::cAlphaArgs(5) == "FRACTIONDISCHARGEDLMTD") {
                    DetailedIceStorage(iceNum).DischargeCurveTypeNum = CurveVars::FracDischargedLMTD;
                } else if (DataIPShortCuts::cAlphaArgs(5) == "LMTDMASSFLOW") {
                    DetailedIceStorage(iceNum).DischargeCurveTypeNum = CurveVars::LMTDMassFlow;
                } else if (DataIPShortCuts::cAlphaArgs(5) == "LMTDFRACTIONCHARGED") {
                    DetailedIceStorage(iceNum).DischargeCurveTypeNum = CurveVars::LMTDFracCharged;
                } else {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject +
                                    ": Discharge curve independent variable options not valid, option=" + DataIPShortCuts::cAlphaArgs(5));
                    ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                    ShowContinueError("The valid options are: FractionChargedLMTD, FractionDischargedLMTD, LMTDMassFlow or LMTDFractionCharged");
                    ErrorsFound = true;
                }
            }

            ErrorsFound |= CurveManager::CheckCurveDims(DetailedIceStorage(iceNum).DischargeCurveNum, // Curve index
                                                        {2},                                          // Valid dimensions
                                                        "GetIceStorageInput: ",                       // Routine name
                                                        DataIPShortCuts::cCurrentModuleObject,        // Object Type
                                                        DetailedIceStorage(iceNum).Name,              // Object Name
                                                        DataIPShortCuts::cAlphaFieldNames(6));        // Field Name

            DetailedIceStorage(iceNum).ChargeCurveName = DataIPShortCuts::cAlphaArgs(8);
            DetailedIceStorage(iceNum).ChargeCurveNum = CurveManager::GetCurveIndex(DataIPShortCuts::cAlphaArgs(8));
            if (DetailedIceStorage(iceNum).ChargeCurveNum <= 0) {
                ShowSevereError("Invalid " + DataIPShortCuts::cAlphaFieldNames(8) + '=' + DataIPShortCuts::cAlphaArgs(8));
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ErrorsFound = true;
            }

            int chargeCurveDim = CurveManager::PerfCurve(DetailedIceStorage(iceNum).ChargeCurveNum).NumDims;
            if (chargeCurveDim != 2) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + ": Charge curve must have 2 independent variables");
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError(DataIPShortCuts::cAlphaArgs(8) +
                                  " does not have 2 independent variables and thus cannot be used for detailed ice storage");
                ErrorsFound = true;
            } else {
                if (DataIPShortCuts::cAlphaArgs(7) == "FRACTIONCHARGEDLMTD") {
                    DetailedIceStorage(iceNum).ChargeCurveTypeNum = CurveVars::FracChargedLMTD;
                } else if (DataIPShortCuts::cAlphaArgs(7) == "FRACTIONDISCHARGEDLMTD") {
                    DetailedIceStorage(iceNum).ChargeCurveTypeNum = CurveVars::FracDischargedLMTD;
                } else if (DataIPShortCuts::cAlphaArgs(7) == "LMTDMASSFLOW") {
                    DetailedIceStorage(iceNum).ChargeCurveTypeNum = CurveVars::LMTDMassFlow;
                } else if (DataIPShortCuts::cAlphaArgs(7) == "LMTDFRACTIONCHARGED") {
                    DetailedIceStorage(iceNum).ChargeCurveTypeNum = CurveVars::LMTDFracCharged;
                } else {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject +
                                    ": Charge curve independent variable options not valid, option=" + DataIPShortCuts::cAlphaArgs(7));
                    ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                    ShowContinueError("The valid options are: FractionChargedLMTD, FractionDischargedLMTD, LMTDMassFlow or LMTDFractionCharged");
                    ErrorsFound = true;
                }
            }

            ErrorsFound |= CurveManager::CheckCurveDims(DetailedIceStorage(iceNum).ChargeCurveNum, // Curve index
                                                        {2},                                       // Valid dimensions
                                                        "GetIceStorageInput: ",                    // Routine name
                                                        DataIPShortCuts::cCurrentModuleObject,     // Object Type
                                                        DetailedIceStorage(iceNum).Name,           // Object Name
                                                        DataIPShortCuts::cAlphaFieldNames(8));     // Field Name

            DetailedIceStorage(iceNum).CurveFitTimeStep = DataIPShortCuts::rNumericArgs(2);
            if ((DetailedIceStorage(iceNum).CurveFitTimeStep <= 0.0) || (DetailedIceStorage(iceNum).CurveFitTimeStep > 1.0)) {
                ShowSevereError("Invalid " + DataIPShortCuts::cNumericFieldNames(2) + '=' +
                                General::RoundSigDigits(DataIPShortCuts::rNumericArgs(2), 3));
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Curve fit time step invalid, less than zero or greater than 1 for " + DataIPShortCuts::cAlphaArgs(1));
                ErrorsFound = true;
            }

            DetailedIceStorage(iceNum).ThawProcessIndicator = DataIPShortCuts::cAlphaArgs(9);
            if (UtilityRoutines::SameString(DetailedIceStorage(iceNum).ThawProcessIndicator, "INSIDEMELT")) {
                DetailedIceStorage(iceNum).ThawProcessIndex = DetIce::InsideMelt;
            } else if ((UtilityRoutines::SameString(DetailedIceStorage(iceNum).ThawProcessIndicator, "OUTSIDEMELT")) ||
                       (DetailedIceStorage(iceNum).ThawProcessIndicator.empty())) {
                DetailedIceStorage(iceNum).ThawProcessIndex = DetIce::OutsideMelt;
            } else {
                ShowSevereError("Invalid thaw process indicator of " + DataIPShortCuts::cAlphaArgs(9) + " was entered");
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError(R"(Value should either be "InsideMelt" or "OutsideMelt")");
                DetailedIceStorage(iceNum).ThawProcessIndex = DetIce::InsideMelt; // Severe error will end simulation, but just in case...
                ErrorsFound = true;
            }

            // Get the other ice storage parameters (electric, heat loss, freezing temperature) and stupidity check each one
            DetailedIceStorage(iceNum).DischargeParaElecLoad = DataIPShortCuts::rNumericArgs(3);
            DetailedIceStorage(iceNum).ChargeParaElecLoad = DataIPShortCuts::rNumericArgs(4);
            DetailedIceStorage(iceNum).TankLossCoeff = DataIPShortCuts::rNumericArgs(5);
            DetailedIceStorage(iceNum).FreezingTemp = DataIPShortCuts::rNumericArgs(6);

            if ((DetailedIceStorage(iceNum).DischargeParaElecLoad < 0.0) || (DetailedIceStorage(iceNum).DischargeParaElecLoad > 1.0)) {
                ShowSevereError("Invalid " + DataIPShortCuts::cNumericFieldNames(3) + '=' +
                                General::RoundSigDigits(DataIPShortCuts::rNumericArgs(3), 3));
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Value is either less than/equal to zero or greater than 1");
                ErrorsFound = true;
            }

            if ((DetailedIceStorage(iceNum).ChargeParaElecLoad < 0.0) || (DetailedIceStorage(iceNum).ChargeParaElecLoad > 1.0)) {
                ShowSevereError("Invalid " + DataIPShortCuts::cNumericFieldNames(4) + '=' +
                                General::RoundSigDigits(DataIPShortCuts::rNumericArgs(4), 3));
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Value is either less than/equal to zero or greater than 1");
                ErrorsFound = true;
            }

            if ((DetailedIceStorage(iceNum).TankLossCoeff < 0.0) || (DetailedIceStorage(iceNum).TankLossCoeff > 0.1)) {
                ShowSevereError("Invalid " + DataIPShortCuts::cNumericFieldNames(5) + '=' +
                                General::RoundSigDigits(DataIPShortCuts::rNumericArgs(5), 3));
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Value is either less than/equal to zero or greater than 0.1 (10%)");
                ErrorsFound = true;
            }

            if ((DetailedIceStorage(iceNum).FreezingTemp < -10.0) || (DetailedIceStorage(iceNum).FreezingTemp > 10.0)) {
                ShowWarningError("Potentially invalid " + DataIPShortCuts::cNumericFieldNames(6) + '=' +
                                 General::RoundSigDigits(DataIPShortCuts::rNumericArgs(6), 3));
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Value is either less than -10.0C or greater than 10.0C");
                ShowContinueError("This value will be allowed but the user should verify that this temperature is correct");
            }

            // Initialize Report Variables
            DetailedIceStorage(iceNum).CompLoad = 0.0;
            DetailedIceStorage(iceNum).IceFracChange = 0.0;
            DetailedIceStorage(iceNum).IceFracRemaining = 1.0;
            DetailedIceStorage(iceNum).IceFracOnCoil = 1.0;
            DetailedIceStorage(iceNum).DischargingRate = 0.0;
            DetailedIceStorage(iceNum).DischargingEnergy = 0.0;
            DetailedIceStorage(iceNum).ChargingRate = 0.0;
            DetailedIceStorage(iceNum).ChargingEnergy = 0.0;
            DetailedIceStorage(iceNum).MassFlowRate = 0.0;
            DetailedIceStorage(iceNum).BypassMassFlowRate = 0.0;
            DetailedIceStorage(iceNum).TankMassFlowRate = 0.0;
            DetailedIceStorage(iceNum).InletTemp = 0.0;
            DetailedIceStorage(iceNum).OutletTemp = 0.0;
            DetailedIceStorage(iceNum).TankOutletTemp = 0.0;
            DetailedIceStorage(iceNum).ParasiticElecRate = 0.0;
            DetailedIceStorage(iceNum).ParasiticElecEnergy = 0.0;

        } // ...over detailed ice storage units

        if ((NumSimpleIceStorage + NumDetailedIceStorage) <= 0) {
            ShowSevereError("No Ice Storage Equipment found in GetIceStorage");
            ErrorsFound = true;
        }

        if (ErrorsFound) {
            ShowFatalError("Errors found in processing input for " + DataIPShortCuts::cCurrentModuleObject);
        }
    }

    void SimpleIceStorageData::setupOutputVars()
    {
        SetupOutputVariable("Ice Thermal Storage Requested Load", OutputProcessor::Unit::W, this->MyLoad, "System", "Average", this->Name);

        SetupOutputVariable("Ice Thermal Storage End Fraction", OutputProcessor::Unit::None, this->IceFracRemain, "Zone", "Average", this->Name);

        SetupOutputVariable("Ice Thermal Storage Mass Flow Rate", OutputProcessor::Unit::kg_s, this->ITSmdot, "System", "Average", this->Name);

        SetupOutputVariable("Ice Thermal Storage Inlet Temperature", OutputProcessor::Unit::C, this->ITSInletTemp, "System", "Average", this->Name);

        SetupOutputVariable("Ice Thermal Storage Outlet Temperature", OutputProcessor::Unit::C, this->ITSOutletTemp, "System", "Average", this->Name);

        SetupOutputVariable(
            "Ice Thermal Storage Cooling Discharge Rate", OutputProcessor::Unit::W, this->ITSCoolingRate_rep, "System", "Average", this->Name);

        SetupOutputVariable(
            "Ice Thermal Storage Cooling Discharge Energy", OutputProcessor::Unit::J, this->ITSCoolingEnergy_rep, "System", "Sum", this->Name);

        SetupOutputVariable(
            "Ice Thermal Storage Cooling Charge Rate", OutputProcessor::Unit::W, this->ITSChargingRate, "System", "Average", this->Name);

        SetupOutputVariable(
            "Ice Thermal Storage Cooling Charge Energy", OutputProcessor::Unit::J, this->ITSChargingEnergy, "System", "Sum", this->Name);
    }

    void DetailedIceStorageData::setupOutputVars()
    {
        SetupOutputVariable("Ice Thermal Storage Cooling Rate", OutputProcessor::Unit::W, this->CompLoad, "System", "Average", this->Name);

        SetupOutputVariable("Ice Thermal Storage Change Fraction", OutputProcessor::Unit::None, this->IceFracChange, "System", "Average", this->Name);

        SetupOutputVariable("Ice Thermal Storage End Fraction", OutputProcessor::Unit::None, this->IceFracRemaining, "System", "Average", this->Name);

        SetupOutputVariable(
            "Ice Thermal Storage On Coil Fraction", OutputProcessor::Unit::None, this->IceFracOnCoil, "System", "Average", this->Name);

        SetupOutputVariable("Ice Thermal Storage Mass Flow Rate", OutputProcessor::Unit::kg_s, this->MassFlowRate, "System", "Average", this->Name);

        SetupOutputVariable(
            "Ice Thermal Storage Bypass Mass Flow Rate", OutputProcessor::Unit::kg_s, this->BypassMassFlowRate, "System", "Average", this->Name);

        SetupOutputVariable(
            "Ice Thermal Storage Tank Mass Flow Rate", OutputProcessor::Unit::kg_s, this->TankMassFlowRate, "System", "Average", this->Name);

        SetupOutputVariable(
            "Ice Thermal Storage Fluid Inlet Temperature", OutputProcessor::Unit::C, this->InletTemp, "System", "Average", this->Name);

        SetupOutputVariable(
            "Ice Thermal Storage Blended Outlet Temperature", OutputProcessor::Unit::C, this->OutletTemp, "System", "Average", this->Name);

        SetupOutputVariable(
            "Ice Thermal Storage Tank Outlet Temperature", OutputProcessor::Unit::C, this->TankOutletTemp, "System", "Average", this->Name);

        SetupOutputVariable(
            "Ice Thermal Storage Cooling Discharge Rate", OutputProcessor::Unit::W, this->DischargingRate, "System", "Average", this->Name);

        SetupOutputVariable(
            "Ice Thermal Storage Cooling Discharge Energy", OutputProcessor::Unit::J, this->DischargingEnergy, "System", "Sum", this->Name);

        SetupOutputVariable("Ice Thermal Storage Cooling Charge Rate", OutputProcessor::Unit::W, this->ChargingRate, "System", "Average", this->Name);

        SetupOutputVariable("Ice Thermal Storage Cooling Charge Energy", OutputProcessor::Unit::J, this->ChargingEnergy, "System", "Sum", this->Name);

        SetupOutputVariable(
            "Ice Thermal Storage Ancillary Electric Power", OutputProcessor::Unit::W, this->ParasiticElecRate, "System", "Average", this->Name);

        SetupOutputVariable("Ice Thermal Storage Ancillary Electric Energy",
                            OutputProcessor::Unit::J,
                            this->ParasiticElecEnergy,
                            "System",
                            "Sum",
                            this->Name,
                            _,
                            "ELECTRICITY",
                            _,
                            _,
                            "System");
    }

    void DetailedIceStorageData::InitDetailedIceStorage()
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Rick Strand
        //       DATE WRITTEN   February 2006
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine initializes variables for the detailed ice storage model.

        // METHODOLOGY EMPLOYED:
        // Initializes parameters based on current status flag values.

        int CompNum; // local do loop index

        if (this->MyPlantScanFlag) {
            bool errFlag = false;
            PlantUtilities::ScanPlantLoopsForObject(this->Name,
                                                    DataPlant::TypeOf_TS_IceDetailed,
                                                    this->PlantLoopNum,
                                                    this->PlantLoopSideNum,
                                                    this->PlantBranchNum,
                                                    this->PlantCompNum,
                                                    errFlag);

            if (errFlag) {
                ShowFatalError("InitDetailedIceStorage: Program terminated due to previous condition(s).");
            }

            this->setupOutputVars();
            this->MyPlantScanFlag = false;
        }

        if (DataGlobals::BeginEnvrnFlag && this->MyEnvrnFlag2) { // Beginning of environment initializations
            // Make sure all state variables are reset at the beginning of every environment to avoid problems.
            // The storage unit is assumed to be fully charged at the start of any environment.
            // The IceNum variable is a module level variable that is already set before this subroutine is called.
            this->IceFracChange = 0.0;
            this->IceFracRemaining = 1.0;
            this->IceFracOnCoil = 1.0;
            this->InletTemp = 0.0;
            this->OutletTemp = 0.0;
            this->TankOutletTemp = 0.0;
            this->DischargeIterErrors = 0;
            this->ChargeIterErrors = 0;
            this->DesignMassFlowRate = DataPlant::PlantLoop(this->PlantLoopNum).MaxMassFlowRate;
            // no design flow rates for model, assume min is zero and max is plant loop's max
            PlantUtilities::InitComponentNodes(0.0,
                                               this->DesignMassFlowRate,
                                               this->PlantInNodeNum,
                                               this->PlantOutNodeNum,
                                               this->PlantLoopNum,
                                               this->PlantLoopSideNum,
                                               this->PlantBranchNum,
                                               this->PlantCompNum);

            if ((DataPlant::PlantLoop(this->PlantLoopNum).CommonPipeType == DataPlant::CommonPipe_TwoWay) &&
                (this->PlantLoopSideNum == DataPlant::SupplySide)) {
                // up flow priority of other components on the same branch as the Ice tank
                for (CompNum = 1;
                     CompNum <= DataPlant::PlantLoop(this->PlantLoopNum).LoopSide(DataPlant::SupplySide).Branch(this->PlantBranchNum).TotalComponents;
                     ++CompNum) {
                    DataPlant::PlantLoop(this->PlantLoopNum).LoopSide(DataPlant::SupplySide).Branch(this->PlantBranchNum).Comp(CompNum).FlowPriority =
                        DataPlant::LoopFlowStatus_NeedyAndTurnsLoopOn;
                }
            }

            this->MyEnvrnFlag2 = false;
        }
        if (!DataGlobals::BeginEnvrnFlag) this->MyEnvrnFlag2 = true;

        // Initializations that are done every iteration
        // Make sure all of the reporting variables are always reset at the start of any iteration
        this->CompLoad = 0.0;
        this->IceFracChange = 0.0;
        this->DischargingRate = 0.0;
        this->DischargingEnergy = 0.0;
        this->ChargingRate = 0.0;
        this->ChargingEnergy = 0.0;
        this->MassFlowRate = 0.0;
        this->BypassMassFlowRate = 0.0;
        this->TankMassFlowRate = 0.0;
        this->ParasiticElecRate = 0.0;
        this->ParasiticElecEnergy = 0.0;
    }

    void SimpleIceStorageData::InitSimpleIceStorage()
    {

        bool errFlag;

        if (this->MyPlantScanFlag) {
            // Locate the storage on the plant loops for later usage
            errFlag = false;
            PlantUtilities::ScanPlantLoopsForObject(
                this->Name, DataPlant::TypeOf_TS_IceSimple, this->LoopNum, this->LoopSideNum, this->BranchNum, this->CompNum, errFlag, _, _, _, _, _);
            if (errFlag) {
                ShowFatalError("InitSimpleIceStorage: Program terminated due to previous condition(s).");
            }

            this->setupOutputVars();
            this->MyPlantScanFlag = false;
        }

        if (DataGlobals::BeginEnvrnFlag && this->MyEnvrnFlag2) {
            this->DesignMassFlowRate = DataPlant::PlantLoop(this->LoopNum).MaxMassFlowRate;
            // no design flow rates for model, assume min is zero and max is plant loop's max
            PlantUtilities::InitComponentNodes(0.0,
                                               this->DesignMassFlowRate,
                                               this->PltInletNodeNum,
                                               this->PltOutletNodeNum,
                                               this->LoopNum,
                                               this->LoopSideNum,
                                               this->BranchNum,
                                               this->CompNum);
            if ((DataPlant::PlantLoop(this->LoopNum).CommonPipeType == DataPlant::CommonPipe_TwoWay) &&
                (this->LoopSideNum == DataPlant::SupplySide)) {
                // up flow priority of other components on the same branch as the Ice tank
                for (int compNum = 1;
                     compNum <= DataPlant::PlantLoop(this->LoopNum).LoopSide(DataPlant::SupplySide).Branch(this->BranchNum).TotalComponents;
                     ++compNum) {
                    DataPlant::PlantLoop(this->LoopNum).LoopSide(DataPlant::SupplySide).Branch(this->BranchNum).Comp(compNum).FlowPriority =
                        DataPlant::LoopFlowStatus_NeedyAndTurnsLoopOn;
                }
            }
            this->MyLoad = 0.0;
            this->Urate = 0.0;
            this->IceFracRemain = 1.0;
            this->ITSCoolingRate = 0.0;
            this->ITSCoolingEnergy_rep = 0.0;
            this->ITSChargingRate = 0.0;
            this->ITSChargingEnergy = 0.0;
            this->ITSmdot = 0.0;
            this->ITSInletTemp = 0.0;
            this->ITSOutletTemp = 0.0;

            this->MyEnvrnFlag2 = false;
        }

        if (!DataGlobals::BeginEnvrnFlag) this->MyEnvrnFlag2 = true;
    }

    //******************************************************************************

    void SimpleIceStorageData::CalcIceStorageCapacity(Real64 &MaxCap, Real64 &MinCap, Real64 &OptCap)
    {
        //------------------------------------------------------------------------
        // FIRST PROCESS (MyLoad = 0.0 as IN)
        // At this moment as first calling of ITS, ITS provide ONLY MaxCap/OptCap/MinCap.
        //------------------------------------------------------------------------

        // Initialize Capacity
        MaxCap = 0.0;
        MinCap = 0.0;
        OptCap = 0.0;

        // XCurIceFrac is reset to 1.0 when first hour of day.
        // Starting full is assumed, because most ice systems are fully charged overnight
        if (this->ResetXForITSFlag) {
            this->XCurIceFrac = 1.0;
            this->IceFracRemain = 1.0;
            this->Urate = 0.0;
            this->ResetXForITSFlag = false;
        }

        // Calculate UAIceDisch[W/C] and UAIceCh[W/F] based on ONLY XCurIceFrac
        this->CalcUAIce(this->XCurIceFrac, this->UAIceCh, this->UAIceDisCh, this->HLoss);

        // Calculate QiceMin by UAIceDisCh*deltaTlm
        //   with UAIceDisCh(function of XCurIceFrac), ITSInletTemp and ITSOutletTemp(=Node(OutletNodeNum)%TempSetPoint by E+[C])
        // QiceMin is REAL(r64) ITS capacity.
        Real64 QiceMin;
        this->CalcQiceDischageMax(QiceMin);

        // At the first call of ITS model, MyLoad is 0. After that proper MyLoad will be provided by E+.
        // Therefore, Umin is decided between input U and ITS REAL(r64) capacity.
        Real64 Umin =
            min(max((-(1.0 - EpsLimitForDisCharge) * QiceMin * TimeInterval / this->ITSNomCap), (-this->XCurIceFrac + EpsLimitForX)), 0.0);

        // Calculate CoolingRate with Uact to provide E+.
        Real64 Uact = Umin;
        Real64 ITSCoolingRateMax = std::abs(Uact * this->ITSNomCap / TimeInterval);
        Real64 ITSCoolingRateOpt = ITSCoolingRateMax;
        Real64 ITSCoolingRateMin = 0.0;

        // Define MaxCap, OptCap, and MinCap
        MaxCap = ITSCoolingRateMax;
        OptCap = ITSCoolingRateOpt;
        MinCap = ITSCoolingRateMin;
    }

    //******************************************************************************

    void SimpleIceStorageData::CalcIceStorageDormant()
    {
        // Provide output results for ITS.
        this->ITSMassFlowRate = 0.0; //[kg/s]

        PlantUtilities::SetComponentFlowRate(
            this->ITSMassFlowRate, this->PltInletNodeNum, this->PltOutletNodeNum, this->LoopNum, this->LoopSideNum, this->BranchNum, this->CompNum);

        this->ITSInletTemp = DataLoopNode::Node(this->PltInletNodeNum).Temp; //[C]
        this->ITSOutletTemp = this->ITSInletTemp;                            //[C]
        {
            auto const SELECT_CASE_var1(DataPlant::PlantLoop(this->LoopNum).LoopDemandCalcScheme);
            if (SELECT_CASE_var1 == DataPlant::SingleSetPoint) {
                this->ITSOutletSetPointTemp = DataLoopNode::Node(this->PltOutletNodeNum).TempSetPoint;
            } else if (SELECT_CASE_var1 == DataPlant::DualSetPointDeadBand) {
                this->ITSOutletSetPointTemp = DataLoopNode::Node(this->PltOutletNodeNum).TempSetPointHi;
            }
        }
        this->ITSCoolingRate = 0.0;   //[W]
        this->ITSCoolingEnergy = 0.0; //[J]

        this->Urate = 0.0; //[n/a]
    }

    //******************************************************************************

    void SimpleIceStorageData::CalcIceStorageCharge()
    {
        //--------------------------------------------------------
        // Initialize
        //--------------------------------------------------------
        // Below values for ITS are reported forCharging process.
        this->ITSMassFlowRate = this->DesignMassFlowRate; //[kg/s]

        PlantUtilities::SetComponentFlowRate(
            this->ITSMassFlowRate, this->PltInletNodeNum, this->PltOutletNodeNum, this->LoopNum, this->LoopSideNum, this->BranchNum, this->CompNum);

        this->ITSInletTemp = DataLoopNode::Node(this->PltInletNodeNum).Temp; //[C]
        this->ITSOutletTemp = this->ITSInletTemp;                            //[C]
        {
            auto const SELECT_CASE_var1(DataPlant::PlantLoop(this->LoopNum).LoopDemandCalcScheme);
            if (SELECT_CASE_var1 == DataPlant::SingleSetPoint) {
                this->ITSOutletSetPointTemp = DataLoopNode::Node(this->PltOutletNodeNum).TempSetPoint;
            } else if (SELECT_CASE_var1 == DataPlant::DualSetPointDeadBand) {
                this->ITSOutletSetPointTemp = DataLoopNode::Node(this->PltOutletNodeNum).TempSetPointHi;
            }
        }
        this->ITSCoolingRate = 0.0;   //[W]
        this->ITSCoolingEnergy = 0.0; //[J]

        // Initialize processed U values
        this->Urate = 0.0;

        // Calculate QiceMax which is REAL(r64) ITS capacity.
        // There are three possible to calculate QiceMax
        //   with ChillerCapacity(Chiller+ITS), ITS capacity(ITS), and QchillerMax(Chiller).
        //--------------------------------------------------------
        // Calcualte QiceMax with QiceMaxByChiller, QiceMaxByITS, QchillerMax
        //--------------------------------------------------------
        // Calculate Qice charge max by Chiller with Twb and UAIceCh
        Real64 QiceMaxByChiller;
        this->CalcQiceChargeMaxByChiller(QiceMaxByChiller); //[W]

        // Chiller is remote now, so chiller out is inlet node temp
        Real64 chillerOutletTemp = DataLoopNode::Node(this->PltInletNodeNum).Temp;
        // Calculate Qice charge max by ITS with ChillerOutletTemp
        Real64 QiceMaxByITS;
        this->CalcQiceChargeMaxByITS(chillerOutletTemp, QiceMaxByITS); //[W]

        // Select minimum as QiceMax
        // Because It is uncertain that QiceMax by chiller is same as QiceMax by ITS.
        Real64 QiceMax = min(QiceMaxByChiller, QiceMaxByITS);

        //--------------------------------------------------------
        // Calculate Umin,Umax,Uact
        //--------------------------------------------------------
        // Set Umin
        // Calculate Umax based on real ITS Max Capacity and remained XCurIceFrac.
        // Umax should be equal or larger than 0.02 for realistic purpose by Dion.
        Real64 Umax =
            max(min(((1.0 - EpsLimitForCharge) * QiceMax * TimeInterval / this->ITSNomCap), (1.0 - this->XCurIceFrac - EpsLimitForX)), 0.0);

        // Cannot charge more than the fraction that is left uncharged
        Umax = min(Umax, (1.0 - this->IceFracRemain) / DataHVACGlobals::TimeStepSys);
        // First, check input U value.
        // Based on Umax and Umin, if necessary to run E+, calculate proper Uact.
        Real64 Uact;
        if (Umax == 0.0) { //(No Capacity of ITS), ITS is OFF.
            Uact = 0.0;

        } else { // Umax non-zero
            Uact = Umax;
        } // Check Uact for Discharging Process

        //--------------------------------------------------------
        // Calcualte possible ITSChargingRate with Uact, Then error check
        //--------------------------------------------------------
        // Calculate possible ITSChargingRate with Uact
        Real64 Qice = Uact * this->ITSNomCap / TimeInterval; //[W]
        // If Qice is equal or less than 0.0, no need to calculate anymore.
        if (Qice <= 0.0) {
            this->Urate = 0.0; //[ratio]
        }

        // Calculate leaving water temperature
        if ((Qice <= 0.0) || (this->XCurIceFrac >= 1.0)) {
            this->ITSOutletTemp = this->ITSInletTemp;
            Qice = 0.0;
            Uact = 0.0;
        } else {
            Real64 DeltaTemp = Qice / Psychrometrics::CPCW(this->ITSInletTemp) / this->ITSMassFlowRate;
            this->ITSOutletTemp = this->ITSInletTemp + DeltaTemp;
            // Limit leaving temp to be no greater than setpoint or freezing temp minus 1C
            this->ITSOutletTemp = min(this->ITSOutletTemp, this->ITSOutletSetPointTemp, (FreezTemp - 1));
            // Limit leaving temp to be no less than inlet temp
            this->ITSOutletTemp = max(this->ITSOutletTemp, this->ITSInletTemp);
            DeltaTemp = this->ITSOutletTemp - this->ITSInletTemp;
            Qice = DeltaTemp * Psychrometrics::CPCW(this->ITSInletTemp) * this->ITSMassFlowRate;
            Uact = Qice / (this->ITSNomCap / TimeInterval);
        } // End of leaving temp checks

        this->Urate = Uact;
        this->ITSCoolingRate = -Qice;
        this->ITSCoolingEnergy = this->ITSCoolingRate * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
    }

    //******************************************************************************

    void SimpleIceStorageData::CalcQiceChargeMaxByChiller(Real64 &QiceMaxByChiller)
    {
        // METHODOLOGY EMPLOYED:
        // Calculation inside is IP unit, then return QiceMaxByChiller as SI [W] unit.

        // Chiller is remote now, so chiller out is inlet node temp
        Real64 TchillerOut = DataLoopNode::Node(this->PltInletNodeNum).Temp;
        QiceMaxByChiller = this->UAIceCh * (FreezTemp - TchillerOut); //[W] = [W/degC]*[degC]

        // If it happened, it is occurred at the Discharging or Dormant process.
        if (QiceMaxByChiller <= 0.0) {
            QiceMaxByChiller = 0.0;
        }
    }

    void SimpleIceStorageData::CalcQiceChargeMaxByITS(Real64 const chillerOutletTemp, // [degC]
                                                      Real64 &QiceMaxByITS            // [W]
    )
    {
        // Qice is maximized when ChillerInletTemp and ChillerOutletTemp(input data) is almost same due to LMTD method.
        // Qice is minimized(=0) when ChillerInletTemp is almost same as FreezTemp(=0).

        // Initilize
        Real64 Tfr = FreezTempIP;
        Real64 ChOutletTemp = TempSItoIP(chillerOutletTemp); //[degF] = ConvertSItoIP[degC]
        // Chiller outlet temp must be below freeze temp, or else no charge
        if (ChOutletTemp >= Tfr) {
            QiceMaxByITS = 0.0;
        } else {
            // Make ChillerInletTemp as almost same as ChillerOutletTemp(input data)
            Real64 ChillerInletTemp = ChOutletTemp + 0.01;
            // ChillerInletTemp cannot be greater than or equal to freeze temp
            if (ChillerInletTemp >= Tfr) {
                ChillerInletTemp = ChOutletTemp + (Tfr - ChOutletTemp) / 2;
            }

            Real64 LogTerm = (Tfr - ChOutletTemp) / (Tfr - ChillerInletTemp);
            // Need to protect this from LogTerm <= 0 - not sure what it should do then
            if (LogTerm <= 0.0) {
                ChillerInletTemp = ChOutletTemp;
                QiceMaxByITS = 0.0;
            }
            QiceMaxByITS = this->UAIceCh * (TempIPtoSI(ChillerInletTemp) - TempIPtoSI(ChOutletTemp)) / std::log(LogTerm);
        }
    }

    void SimpleIceStorageData::CalcIceStorageDischarge(Real64 const myLoad, // operating load
                                                       bool const RunFlag,  // TRUE when ice storage operating
                                                       Real64 const MaxCap  // Max possible discharge rate (positive value)
    )
    {
        static constexpr std::string_view RoutineName("SimpleIceStorageData::CalcIceStorageDischarge");

        // Initialize processed Rate and Energy
        this->ITSMassFlowRate = 0.0;
        this->ITSCoolingRate = 0.0;
        this->ITSCoolingEnergy = 0.0;

        {
            auto const SELECT_CASE_var1(DataPlant::PlantLoop(this->LoopNum).LoopDemandCalcScheme);
            if (SELECT_CASE_var1 == DataPlant::SingleSetPoint) {
                this->ITSOutletSetPointTemp = DataLoopNode::Node(this->PltOutletNodeNum).TempSetPoint;
            } else if (SELECT_CASE_var1 == DataPlant::DualSetPointDeadBand) {
                this->ITSOutletSetPointTemp = DataLoopNode::Node(this->PltOutletNodeNum).TempSetPointHi;
            }
        }

        // Initialize processed U values
        this->Urate = 0.0;

        // If no component demand or ITS OFF, then RETURN.
        if (myLoad == 0 || !RunFlag) {
            this->ITSMassFlowRate = 0.0;
            this->ITSInletTemp = DataLoopNode::Node(this->PltInletNodeNum).Temp;
            this->ITSOutletTemp = this->ITSInletTemp;
            this->ITSCoolingRate = 0.0;
            this->ITSCoolingEnergy = 0.0;
            return;
        }

        // If FlowLock(provided by PlantSupplyManager) is False(=0), that is, MyLoad is not changed.
        // then based on MyLoad, new ITSMassFlowRate will be calculated.

        //----------------------------
        int loopNum = this->LoopNum;

        Real64 CpFluid = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(loopNum).FluidName,
                                                           DataLoopNode::Node(this->PltInletNodeNum).Temp,
                                                           DataPlant::PlantLoop(loopNum).FluidIndex,
                                                           RoutineName);

        // Calculate Umyload based on MyLoad from E+
        Real64 Umyload = -myLoad * TimeInterval / this->ITSNomCap;
        // Calculate Umax and Umin
        // Cannot discharge more than the fraction that is left
        Real64 Umax = -this->IceFracRemain / DataHVACGlobals::TimeStepSys;
        // Calculate Umin based on returned MyLoad from E+.
        Real64 Umin = min(Umyload, 0.0);
        // Based on Umax and Umin, if necessary to run E+, calculate proper Uact
        // U is negative here.
        Real64 Uact = max(Umin, Umax);

        // Set ITSInletTemp provided by E+
        this->ITSInletTemp = DataLoopNode::Node(this->PltInletNodeNum).Temp;
        // The first thing is to set the ITSMassFlowRate
        this->ITSMassFlowRate = this->DesignMassFlowRate; //[kg/s]

        PlantUtilities::SetComponentFlowRate(
            this->ITSMassFlowRate, this->PltInletNodeNum, this->PltOutletNodeNum, this->LoopNum, this->LoopSideNum, this->BranchNum, this->CompNum);

        // Qice is calculate input U which is within boundary between Umin and Umax.
        Real64 Qice = Uact * this->ITSNomCap / TimeInterval;
        // Qice cannot exceed MaxCap calulated by CalcIceStorageCapacity
        // Note Qice is negative here, MaxCap is positive
        Qice = max(Qice, -MaxCap);

        // Calculate leaving water temperature
        if ((Qice >= 0.0) || (this->XCurIceFrac <= 0.0) || (this->ITSMassFlowRate < DataBranchAirLoopPlant::MassFlowTolerance)) {
            this->ITSOutletTemp = this->ITSInletTemp;
            Qice = 0.0;
            Uact = 0.0;
        } else {
            Real64 DeltaTemp = Qice / CpFluid / this->ITSMassFlowRate;
            this->ITSOutletTemp = this->ITSInletTemp + DeltaTemp;
            // Limit leaving temp to be no less than setpoint or freezing temp plus 1C
            this->ITSOutletTemp = max(this->ITSOutletTemp, this->ITSOutletSetPointTemp, (FreezTemp + 1));
            // Limit leaving temp to be no greater than inlet temp
            this->ITSOutletTemp = min(this->ITSOutletTemp, this->ITSInletTemp);
            DeltaTemp = this->ITSOutletTemp - this->ITSInletTemp;
            Qice = DeltaTemp * CpFluid * this->ITSMassFlowRate;
            Uact = Qice / (this->ITSNomCap / TimeInterval);
        } // End of leaving temp checks

        // Calculate reported U value
        this->Urate = Uact;
        // Calculate ITSCoolingEnergy [J]
        this->ITSCoolingRate = -Qice;
        this->ITSCoolingEnergy = this->ITSCoolingRate * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
    }

    void SimpleIceStorageData::CalcQiceDischageMax(Real64 &QiceMin)
    {

        // Qice is minimized when ITSInletTemp and ITSOutletTemp is almost same due to LMTD method.
        // Qice is maximized(=0) when ITSOutletTemp is almost same as FreezTemp(=0).

        Real64 ITSInletTemp_loc = DataLoopNode::Node(this->PltInletNodeNum).Temp;
        Real64 ITSOutletTemp_loc = 0.0;
        {
            auto const SELECT_CASE_var(DataPlant::PlantLoop(this->LoopNum).LoopDemandCalcScheme);
            if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                ITSOutletTemp_loc = DataLoopNode::Node(this->PltOutletNodeNum).TempSetPoint;
            } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                ITSOutletTemp_loc = DataLoopNode::Node(this->PltOutletNodeNum).TempSetPointHi;
            } else {
                assert(false);
            }
        }

        Real64 LogTerm = (ITSInletTemp_loc - FreezTemp) / (ITSOutletTemp_loc - FreezTemp);

        if (LogTerm <= 1) {
            QiceMin = 0.0;
        } else {
            QiceMin = this->UAIceDisCh * (ITSInletTemp_loc - ITSOutletTemp_loc) / std::log(LogTerm);
        }
    }

    void SimpleIceStorageData::CalcUAIce(Real64 const XCurIceFrac_loc, Real64 &UAIceCh_loc, Real64 &UAIceDisCh_loc, Real64 &HLoss_loc)
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR
        //       DATE WRITTEN
        //       MODIFIED
        //       RE-ENGINEERED

        // PURPOSE OF THIS SUBROUTINE:

        // METHODOLOGY EMPLOYED:
        // This routine is function of XCurIceFrac, and UA value is based on 1 hour.

        {
            auto const SELECT_CASE_var(this->ITSType_Num);
            if (SELECT_CASE_var == ITSType::IceOnCoilInternal) {
                Real64 y = XCurIceFrac_loc;
                UAIceCh_loc = (1.3879 - 7.6333 * y + 26.3423 * pow_2(y) - 47.6084 * pow_3(y) + 41.8498 * pow_4(y) - 14.2948 * pow_5(y)) *
                              this->ITSNomCap / TimeInterval / 10.0; // [W/C]
                y = 1.0 - XCurIceFrac_loc;
                UAIceDisCh_loc = (1.3879 - 7.6333 * y + 26.3423 * pow_2(y) - 47.6084 * pow_3(y) + 41.8498 * pow_4(y) - 14.2948 * pow_5(y)) *
                                 this->ITSNomCap / TimeInterval / 10.0; // [W/C]
                HLoss_loc = 0.0;
            } else if (SELECT_CASE_var == ITSType::IceOnCoilExternal) {
                Real64 y = XCurIceFrac_loc;
                UAIceCh_loc = (1.3879 - 7.6333 * y + 26.3423 * pow_2(y) - 47.6084 * pow_3(y) + 41.8498 * pow_4(y) - 14.2948 * pow_5(y)) *
                              this->ITSNomCap / TimeInterval / 10.0; // [W/C]
                y = 1.0 - XCurIceFrac_loc;
                UAIceDisCh_loc = (1.1756 - 5.3689 * y + 17.3602 * pow_2(y) - 30.1077 * pow_3(y) + 25.6387 * pow_4(y) - 8.5102 * pow_5(y)) *
                                 this->ITSNomCap / TimeInterval / 10.0; // [W/C]
                HLoss_loc = 0.0;
            }
        }
    }

    Real64 CalcDetIceStorLMTDstar(Real64 const Tin,  // ice storage unit inlet temperature
                                  Real64 const Tout, // ice storage unit outlet (setpoint) temperature
                                  Real64 const Tfr   // freezing temperature
    )
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Rick Strand
        //       DATE WRITTEN   February 2006
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine calculates the log mean temperature difference for
        // the detailed ice storage unit.  The temperature difference is non-
        // dimensionalized using a nominal temperature difference of 10C.
        // This value must be used when obtaining the curve fit coefficients.

        // METHODOLOGY EMPLOYED:
        // Straight-forward calculation where:
        // LMTD* = LMTD/Tnom
        // LMTD = (Tin-Tout)/ln((Tin-Tfr)/(Tout-Tfr))

        Real64 CalcDetIceStorLMTDstar;
        Real64 const Tnom(10.0); // Nominal temperature difference across the ice storage unit [C]

        // First set the temperature differences and avoid problems with the LOG
        // term by setting some reasonable minimums
        Real64 DeltaTio = std::abs(Tin - Tout); // Inlet to outlet temperature difference
        Real64 DeltaTif = std::abs(Tin - Tfr);  // Inlet to freezing temperature difference
        Real64 DeltaTof = std::abs(Tout - Tfr); // Outlet to freezing temperature difference

        if (DeltaTif < DeltaTifMin) DeltaTif = DeltaTifMin;
        if (DeltaTof < DeltaTofMin) DeltaTof = DeltaTofMin;

        CalcDetIceStorLMTDstar = (DeltaTio / std::log(DeltaTif / DeltaTof)) / Tnom;

        return CalcDetIceStorLMTDstar;
    }

    Real64 CalcQstar(int const CurveIndex,      // curve index
                     enum CurveVars CurveIndVarType, // independent variable type for ice storage
                     Real64 const FracCharged,  // fraction charged for ice storage unit
                     Real64 const LMTDstar,     // normalized log mean temperature difference across the ice storage unit
                     Real64 const MassFlowstar  // normalized mass flow rate through the ice storage unit
    )
    {

        Real64 CalcQstar;

        if (CurveIndVarType == CurveVars::FracChargedLMTD) {
            CalcQstar = std::abs(CurveManager::CurveValue(CurveIndex, FracCharged, LMTDstar));
        } else if (CurveIndVarType == CurveVars::FracDischargedLMTD) {
            CalcQstar = std::abs(CurveManager::CurveValue(CurveIndex, (1.0 - FracCharged), LMTDstar));
        } else if (CurveIndVarType == CurveVars::LMTDMassFlow) {
            CalcQstar = std::abs(CurveManager::CurveValue(CurveIndex, LMTDstar, MassFlowstar));
        } else if (CurveIndVarType == CurveVars::LMTDFracCharged) {
            CalcQstar = std::abs(CurveManager::CurveValue(CurveIndex, LMTDstar, FracCharged));
        } else { // should never get here as this is checked on input
            CalcQstar = 0.0;
        }

        return CalcQstar;
    }

    Real64 TempSItoIP(Real64 const Temp)
    {
        return (Temp * 9.0 / 5.0) + 32.0;
    }

    Real64 TempIPtoSI(Real64 const Temp)
    {
        return (Temp - 32.0) * 5.0 / 9.0;
    }

    void SimpleIceStorageData::UpdateNode(Real64 const myLoad, bool const RunFlag)
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Dan Fisher
        //       DATE WRITTEN:    October 1998

        // Update Node Inlet & Outlet MassFlowRat
        PlantUtilities::SafeCopyPlantNode(this->PltInletNodeNum, this->PltOutletNodeNum);
        if (myLoad == 0 || !RunFlag) {
            // Update Outlet Conditions so that same as Inlet, so component can be bypassed if necessary
            DataLoopNode::Node(this->PltOutletNodeNum).Temp = DataLoopNode::Node(this->PltInletNodeNum).Temp;
        } else {
            DataLoopNode::Node(this->PltOutletNodeNum).Temp = this->ITSOutletTemp;
        }
    }

    void SimpleIceStorageData::RecordOutput(Real64 const myLoad, bool const RunFlag)
    {
        if (myLoad == 0 || !RunFlag) {
            this->MyLoad = myLoad;
            this->ITSCoolingRate_rep = 0.0;
            this->ITSCoolingEnergy_rep = 0.0;
            this->ITSChargingRate = 0.0;
            this->ITSChargingEnergy = 0.0;
            this->ITSmdot = 0.0;

        } else {
            this->MyLoad = myLoad;
            if (this->ITSCoolingRate > 0.0) {
                this->ITSCoolingRate_rep = this->ITSCoolingRate;
                this->ITSCoolingEnergy_rep = this->ITSCoolingEnergy;
                this->ITSChargingRate = 0.0;
                this->ITSChargingEnergy = 0.0;
            } else {
                this->ITSCoolingRate_rep = 0.0;
                this->ITSCoolingEnergy_rep = 0.0;
                this->ITSChargingRate = -this->ITSCoolingRate;
                this->ITSChargingEnergy = -this->ITSCoolingEnergy;
            }
            this->ITSmdot = this->ITSMassFlowRate;
        }
    }

    void UpdateIceFractions()
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Mike Witte
        //       DATE WRITTEN   September 2005
        //       MODIFIED       Rick Strand (Feb 2006, for detailed ice storage model)
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // Update all ice fractions at end of system time step.

        // METHODOLOGY EMPLOYED:
        // This is called from HVACManager once we have actually stepped forward
        // a system time step.

        for (auto &thisITS : SimpleIceStorage) {
            thisITS.IceFracRemain += thisITS.Urate * DataHVACGlobals::TimeStepSys;
            if (thisITS.IceFracRemain <= 0.001) thisITS.IceFracRemain = 0.0;
            if (thisITS.IceFracRemain > 1.0) thisITS.IceFracRemain = 1.0;
        }

        for (auto &thisITS : DetailedIceStorage) {
            thisITS.IceFracRemaining += thisITS.IceFracChange - (thisITS.TankLossCoeff * DataHVACGlobals::TimeStepSys);
            if (thisITS.IceFracRemaining < 0.001) thisITS.IceFracRemaining = 0.0;
            if (thisITS.IceFracRemaining > 1.000) thisITS.IceFracRemaining = 1.0;
            // Reset the ice on the coil to zero for inside melt whenever discharging takes place.
            // This assumes that any remaining ice floats away from the coil and resettles perfectly.
            // While this is not exactly what happens and it is possible theoretically to have multiple
            // freeze thaw cycles that are not complete, this is the best we can do.
            if (thisITS.ThawProcessIndex == DetIce::InsideMelt) {
                if (thisITS.IceFracChange < 0.0) {
                    thisITS.IceFracOnCoil = 0.0;
                } else {
                    // Assume loss term does not impact ice on the coil but what is remaining
                    thisITS.IceFracOnCoil += thisITS.IceFracChange;
                    // If the ice remaining has run out because of tank losses, reset ice fraction on coil so that it keeps track of losses
                    if (thisITS.IceFracOnCoil > thisITS.IceFracRemaining) thisITS.IceFracOnCoil = thisITS.IceFracRemaining;
                }
            } else { // Outside melt system so IceFracOnCoil is always the same as IceFracRemaining (needs to be done for reporting only)
                thisITS.IceFracOnCoil = thisITS.IceFracRemaining;
            }
        }
    }

    void DetailedIceStorageData::UpdateDetailedIceStorage()
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Rick Strand
        //       DATE WRITTEN   February 2006
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine takes the necessary information from the local data
        // structure and moves it back to the loop node data structure.

        // METHODOLOGY EMPLOYED:
        // Not much mystery here--just move the data to the appropriate place
        // for the detailed ice storage system in question.

        // Set the temperature and flow rate for the component outlet node
        int InNodeNum = this->PlantInNodeNum;
        int OutNodeNum = this->PlantOutNodeNum;

        PlantUtilities::SafeCopyPlantNode(InNodeNum, OutNodeNum);

        DataLoopNode::Node(OutNodeNum).Temp = this->OutletTemp;
    }

    void DetailedIceStorageData::ReportDetailedIceStorage()
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Rick Strand
        //       DATE WRITTEN   February 2006
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine reports all of the output necessary for the model.

        // METHODOLOGY EMPLOYED:
        // Just take what has already been calculated or calculate the appropriate
        // output value based on simulation data.

        Real64 const LowLoadLimit(0.1); // Load below which device can be assumed off [W]

        if (this->CompLoad < LowLoadLimit) { // No load condition

            this->IceFracChange = 0.0;
            this->DischargingRate = 0.0;
            this->DischargingEnergy = 0.0;
            this->ChargingRate = 0.0;
            this->ChargingEnergy = 0.0;
            this->ParasiticElecRate = 0.0;
            this->ParasiticElecEnergy = 0.0;

        } else { // There is a load, determine whether we are charging or discharging based on inlet and outlet temperature

            if (this->InletTemp < this->OutletTemp) { // Charging Mode

                this->ChargingRate = this->CompLoad;
                this->ChargingEnergy = this->CompLoad * (DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour);
                this->IceFracChange = this->CompLoad * DataHVACGlobals::TimeStepSys / this->NomCapacity;
                this->DischargingRate = 0.0;
                this->DischargingEnergy = 0.0;
                this->ParasiticElecRate = this->ChargeParaElecLoad * this->CompLoad;
                this->ParasiticElecEnergy = this->ChargeParaElecLoad * this->ChargingEnergy;

            } else { // (DetailedIceStorage(IceNum)%InletTemp < DetailedIceStorage(IceNum)%OutletTemp) Discharging Mode

                this->DischargingRate = this->CompLoad;
                this->DischargingEnergy = this->CompLoad * (DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour);
                this->IceFracChange = -this->CompLoad * DataHVACGlobals::TimeStepSys / this->NomCapacity;
                this->ChargingRate = 0.0;
                this->ChargingEnergy = 0.0;
                this->ParasiticElecRate = this->DischargeParaElecLoad * this->CompLoad;
                this->ParasiticElecEnergy = this->DischargeParaElecLoad * this->ChargingEnergy;
            }
        }
    }

} // namespace IceThermalStorage

} // namespace EnergyPlus
