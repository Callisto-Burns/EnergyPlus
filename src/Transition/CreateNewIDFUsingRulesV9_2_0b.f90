MODULE SetVersion

USE DataStringGlobals
USE DataVCompareGlobals

PUBLIC

CONTAINS

SUBROUTINE SetThisVersionVariables()
      VerString='Conversion 9.2 => 9.2b'
      VersionNum=9.2
      sVersionNum='9.2'
      IDDFileNameWithPath=TRIM(ProgramPath)//'V9-2-0-Energy+Old.idd'
      NewIDDFileNameWithPath=TRIM(ProgramPath)//'V9-2-0-Energy+.idd'
      RepVarFileNameWithPath=TRIM(ProgramPath)//'Report Variables 9-1-0 to 9-2-0.csv'
END SUBROUTINE

END MODULE

SUBROUTINE CreateNewIDFUsingRules(EndOfFile,DiffOnly,InLfn,AskForInput,InputFileName,ArgFile,ArgIDFExtension)

          ! SUBROUTINE INFORMATION:
          !       AUTHOR         Linda Lawrie
          !       DATE WRITTEN   July 2002
          !       MODIFIED       For each release
          !       RE-ENGINEERED  na

          ! PURPOSE OF THIS SUBROUTINE:
          ! This subroutine creates new IDFs based on the rules specified by
          ! developers.  This will result in a more complete transition but
          ! takes more time to create.

          ! METHODOLOGY EMPLOYED:
          ! na

          ! REFERENCES:
          ! na

          ! USE STATEMENTS:
  USE InputProcessor
  USE DataVCompareGlobals
  USE VCompareGlobalRoutines
  USE DataStringGlobals, ONLY: ProgNameConversion
  USE General
  USE DataGlobals, ONLY: ShowMessage, ShowContinueError, ShowFatalError, ShowSevereError, ShowWarningError

  IMPLICIT NONE    ! Enforce explicit typing of all variables in this routine

          ! SUBROUTINE ARGUMENT DEFINITIONS:
  LOGICAL, INTENT(INOUT) :: EndOfFile
  LOGICAL, INTENT(IN)    :: DiffOnly
  INTEGER, INTENT(IN)    :: InLfn
  LOGICAL, INTENT(IN)    :: AskForInput
  CHARACTER(len=*), INTENT(IN) :: InputFileName
  LOGICAL, INTENT(IN)    :: ArgFile
  CHARACTER(len=*), INTENT(IN) :: ArgIDFExtension

          ! SUBROUTINE PARAMETER DEFINITIONS:
  CHARACTER(len=*), PARAMETER :: fmta="(A)"

          ! INTERFACE BLOCK SPECIFICATIONS
          ! na

          ! DERIVED TYPE DEFINITIONS

          ! SUBROUTINE LOCAL VARIABLE DECLARATIONS:
  INTEGER IoS
  INTEGER DotPos
  INTEGER Status
  INTEGER NA
  INTEGER NN
  INTEGER CurArgs
  INTEGER DifLfn
  INTEGER xCount
  INTEGER Num
  INTEGER, EXTERNAL :: GetNewUnitNumber
  INTEGER Arg
  LOGICAL, SAVE :: FirstTime=.true.
  CHARACTER(len=30) UnitsArg
  CHARACTER(len=MaxNameLength) ::  ObjectName
  CHARACTER(len=30), EXTERNAL :: TrimTrailZeros
  CHARACTER(len=MaxNameLength) ::  UCRepVarName=blank
  CHARACTER(len=MaxNameLength) ::  UCCompRepVarName=blank
  LOGICAL DelThis
  INTEGER pos
  INTEGER pos2
  LOGICAL ExitBecauseBadFile
  LOGICAL StillWorking
  LOGICAL NoDiff
  LOGICAL checkrvi
  LOGICAL NoVersion
  LOGICAL DiffMinFields  ! Set to true when diff number of min-fields between the two objects
  LOGICAL Written
  INTEGER :: Var
  INTEGER :: CurVar
  LOGICAL ArgFileBeingDone
  LOGICAL LatestVersion
  CHARACTER(len=10) :: LocalFileExtension=' '
  LOGICAL :: WildMatch

  LOGICAL :: ConnComp
  LOGICAL :: ConnCompCtrl
  LOGICAL :: FileExist
  CHARACTER(len=MaxNameLength) :: CreatedOutputName
  LOGICAL, ALLOCATABLE, DIMENSION(:) :: DeleteThisRecord
  INTEGER :: COutArgs
  CHARACTER(len=16) :: UnitsField
  LOGICAL :: ScheduleTypeLimitsAnyNumber
  LOGICAL :: cycling
  LOGICAL :: continuous
  CHARACTER(len=MaxNameLength) :: OutScheduleName
  LOGICAL :: isDElightOutVar

  REAL :: GLHETempVal = 0.0
  INTEGER :: TempArgsNum = 0

  LOGICAL :: ErrFlag

  INTEGER :: I, CurField, NewField, KAindex=0, SearchNum
  INTEGER :: AlphaNumI
  REAL :: SaveNumber
  CHARACTER(len=MaxNameLength*2), ALLOCATABLE, DIMENSION(:) :: PFldNames
  CHARACTER(len=MaxNameLength), ALLOCATABLE, DIMENSION(:) :: PFldDefaults
  CHARACTER(len=20), ALLOCATABLE, DIMENSION(:) :: PFldUnits
  INTEGER PObjMinFlds
  LOGICAL, ALLOCATABLE, DIMENSION(:) :: PAOrN
  LOGICAL, ALLOCATABLE, DIMENSION(:) :: PReqFld
  INTEGER PNumArgs   ! Number of Arguments in a definition
  CHARACTER(len=MaxNameLength), ALLOCATABLE, DIMENSION(:) :: POutArgs


  ! For Defaulting now-required RunPeriod Name
  INTEGER :: TotRunPeriods = 0
  INTEGER :: runPeriodNum = 0
  INTEGER :: iterateRunPeriod = 0
  CHARACTER(len=MaxNameLength), ALLOCATABLE, DIMENSION(:) :: CurrentRunPeriodNames
  CHARACTER(len=20) :: PotentialRunPeriodName

  ! Only needed for ZoneHVAC:EquipmentList translation
  INTEGER zeqNum
  CHARACTER(len=20) :: zeqNumStr
  CHARACTER(len=7) :: zeqHeatingOrCooling
  LOGICAL :: writeScheduleTypeObj = .true.
  
  ! For AirTerminal:SingleDuct:Uncontrolled transition
  CHARACTER(len=MaxNameLength), ALLOCATABLE, DIMENSION(:) :: ATSDUNodeNames
  CHARACTER(len=MaxNameLength), ALLOCATABLE, DIMENSION(:) :: MatchingATSDUAirFlowNodeNames
  INTEGER TotATSDUObjs
  INTEGER TotAFNDistNodeObjs
  INTEGER atCount
  INTEGER nodeCount
  INTEGER TotNodeListOjbs
  INTEGER nodeListCount
  INTEGER nodeCountA
  LOGICAL :: nodeFound = .false.

  If (FirstTime) THEN  ! do things that might be applicable only to this new version
    FirstTime=.false.
  EndIf

  StillWorking=.true.
  ArgFileBeingDone=.false.
  LatestVersion=.false.
  NoVersion=.true.
  LocalFileExtension=ArgIDFExtension
  EndOfFile=.false.
  IOS=0

  DO WHILE (StillWorking)

    ExitBecauseBadFile=.false.
    DO WHILE (.not. EndOfFile)
      IF (AskForInput) THEN
        WRITE(*,*) 'Enter input file name, with path'
        write(*,fmta,advance='no') '-->'
        READ(*,fmta) FullFileName
      ELSE
        IF (.not. ArgFile) THEN
          READ(InLfn,*,IOSTAT=IoS) FullFileName
        ELSEIF (.not. ArgFileBeingDone) THEN
          FullFileName=InputFileName
          IOS=0
          ArgFileBeingDone=.true.
        ELSE
          FullFileName=Blank
          IOS=1
        ENDIF
        IF (FullFileName(1:1) == '!') THEN
          FullFileName=Blank
          CYCLE
        ENDIF
      ENDIF
      UnitsArg=Blank
      IF (IoS /= 0) FullFileName=Blank
      FullFileName=ADJUSTL(FullFileName)
      IF (FullFileName /= Blank) THEN
        CALL DisplayString('Processing IDF -- '//TRIM(FullFileName))
        WRITE(Auditf,fmta) ' Processing IDF -- '//TRIM(FullFileName)
        DotPos=SCAN(FullFileName,'.',.true.) ! Scan backward looking for extension,
        IF (DotPos /= 0) THEN
          FileNamePath=FullFileName(1:DotPos-1)
          LocalFileExtension=MakeLowerCase(FullFileName(DotPos+1:))
        ELSE
          FileNamePath=FullFileName
          WRITE(*,*) ' assuming file extension of .idf'
          WRITE(Auditf,fmta) ' ..assuming file extension of .idf'
          FullFileName=TRIM(FullFileName)//'.idf'
          LocalFileExtension='idf'
        ENDIF
        ! Process the old input
        DifLfn=GetNewUnitNumber()
        INQUIRE(File=TRIM(FullFileName),EXIST=FileOK)
        IF (.not. FileOK) THEN
          WRITE(*,*) 'File not found='//TRIM(FullFileName)
          WRITE(Auditf,*) 'File not found='//TRIM(FullFileName)
          EndOfFile=.true.
          ExitBecauseBadFile=.true.
          EXIT
        ENDIF
        IF (LocalFileExtension == 'idf' .or. LocalFileExtension == 'imf') THEN
          checkrvi=.false.
          ConnComp=.false.
          ConnCompCtrl=.false.
          IF (DiffOnly) THEN
            OPEN(DifLfn,FILE=TRIM(FileNamePath)//'.'//TRIM(LocalFileExtension)//'dif')
          ELSE
            OPEN(DifLfn,FILE=TRIM(FileNamePath)//'.'//TRIM(LocalFileExtension)//'new')
          ENDIF
          IF (LocalFileExtension == 'imf') THEN
            CALL ShowWarningError('Note: IMF file being processed.  No guarantee of perfection.  Please check new file carefully.',Auditf)
            ProcessingIMFFile=.true.
          ELSE
            ProcessingIMFFile=.false.
          ENDIF
          CALL ProcessInput(IDDFileNameWithPath,NewIDDFileNameWithPath,FullFileName)
          IF (FatalError) THEN
            ExitBecauseBadFile=.true.
            EXIT
          ENDIF

          ! Clean up from any previous passes, then re-allocate
          IF(ALLOCATED(DeleteThisRecord)) DEALLOCATE(DeleteThisRecord)
          IF(ALLOCATED(Alphas)) DEALLOCATE(Alphas)
          IF(ALLOCATED(Numbers)) DEALLOCATE(Numbers)
          IF(ALLOCATED(InArgs)) DEALLOCATE(InArgs)
          IF(ALLOCATED(TempArgs)) DEALLOCATE(TempArgs)
          IF(ALLOCATED(AorN)) DEALLOCATE(AorN)
          IF(ALLOCATED(ReqFld)) DEALLOCATE(ReqFld)
          IF(ALLOCATED(FldNames)) DEALLOCATE(FldNames)
          IF(ALLOCATED(FldDefaults)) DEALLOCATE(FldDefaults)
          IF(ALLOCATED(FldUnits)) DEALLOCATE(FldUnits)
          IF(ALLOCATED(NwAorN)) DEALLOCATE(NwAorN)
          IF(ALLOCATED(NwReqFld)) DEALLOCATE(NwReqFld)
          IF(ALLOCATED(NwFldNames)) DEALLOCATE(NwFldNames)
          IF(ALLOCATED(NwFldDefaults)) DEALLOCATE(NwFldDefaults)
          IF(ALLOCATED(NwFldUnits)) DEALLOCATE(NwFldUnits)
          IF(ALLOCATED(OutArgs)) DEALLOCATE(OutArgs)
          IF(ALLOCATED(PAorN)) DEALLOCATE(PAorN)
          IF(ALLOCATED(PReqFld)) DEALLOCATE(PReqFld)
          IF(ALLOCATED(PFldNames)) DEALLOCATE(PFldNames)
          IF(ALLOCATED(PFldDefaults)) DEALLOCATE(PFldDefaults)
          IF(ALLOCATED(PFldUnits)) DEALLOCATE(PFldUnits)
          IF(ALLOCATED(POutArgs)) DEALLOCATE(POutArgs)
          IF(ALLOCATED(MatchArg)) DEALLOCATE(MatchArg)
          ALLOCATE(Alphas(MaxAlphaArgsFound),Numbers(MaxNumericArgsFound))
          ALLOCATE(InArgs(MaxTotalArgs))
          ALLOCATE(TempArgs(MaxTotalArgs))
          ALLOCATE(AorN(MaxTotalArgs),ReqFld(MaxTotalArgs),FldNames(MaxTotalArgs),FldDefaults(MaxTotalArgs),FldUnits(MaxTotalArgs))
          ALLOCATE(NwAorN(MaxTotalArgs),NwReqFld(MaxTotalArgs),NwFldNames(MaxTotalArgs),NwFldDefaults(MaxTotalArgs),NwFldUnits(MaxTotalArgs))
          ALLOCATE(PAorN(MaxTotalArgs),PReqFld(MaxTotalArgs),PFldNames(MaxTotalArgs),PFldDefaults(MaxTotalArgs),PFldUnits(MaxTotalArgs))
          ALLOCATE(OutArgs(MaxTotalArgs))
          ALLOCATE(POutArgs(MaxTotalArgs))
          ALLOCATE(MatchArg(MaxTotalArgs))
          ALLOCATE(DeleteThisRecord(NumIDFRecords))
          DeleteThisRecord=.false.

          NoVersion=.true.
          DO Num=1,NumIDFRecords
            IF (MakeUPPERCase(IDFRecords(Num)%Name) /= 'VERSION') CYCLE
            NoVersion=.false.
            EXIT
          ENDDO

          ScheduleTypeLimitsAnyNumber=.false.
          DO Num=1,NumIDFRecords
            IF (.not. SameString(IDFRecords(Num)%Name,'ScheduleTypeLimits')) CYCLE
            IF (.not. SameString(IDFRecords(Num)%Alphas(1),'Any Number')) CYCLE
            ScheduleTypeLimitsAnyNumber=.true.
            EXIT
          ENDDO

          DO Num=1,NumIDFRecords
            IF (DeleteThisRecord(Num)) THEN
              Write(DifLfn,fmta) '! Deleting: '//TRIM(IDFRecords(Num)%Name)//'="'//TRIM(IDFRecords(Num)%Alphas(1))//'".'
            ENDIF
          ENDDO

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!                                                    P R E P R O C E S S I N G                                                     !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

     ! Begin Pre-process AirTerminal:SingleDuct:Uncontrolled
          ! Clean up from any previous passes, then re-allocate
          IF(ALLOCATED(ATSDUNodeNames)) DEALLOCATE(ATSDUNodeNames)
          IF(ALLOCATED(MatchingATSDUAirFlowNodeNames)) DEALLOCATE(MatchingATSDUAirFlowNodeNames)
          TotATSDUObjs = GetNumObjectsFound('AIRTERMINAL:SINGLEDUCT:UNCONTROLLED')
          IF (TotATSDUObjs > 0) ALLOCATE(ATSDUNodeNames(TotATSDUObjs))
          TotAFNDistNodeObjs = GetNumObjectsFound('AIRFLOWNETWORK:DISTRIBUTION:NODE')
          IF (TotAFNDistNodeObjs > 0) ALLOCATE(MatchingATSDUAirFlowNodeNames(TotATSDUObjs))

          DO atCount=1, TotATSDUObjs
            CALL GetObjectItem('AIRTERMINAL:SINGLEDUCT:UNCONTROLLED',atCount,Alphas,NumAlphas,Numbers,NumNumbers,Status)
            ! Store Zone Supply Air Node Name to search and replace later in AirLoopHVAC:ZoneSplitter and AirLoopHVAC:SupplyPlenum objects
            ATSDUNodeNames(atCount) = Alphas(3)
            IF (TotATSDUObjs > 0) THEN
              ! Find and Store name of corresponding AIRFLOWNETWORK:DISTRIBUTION:NODE object to search for later in AIRFLOWNETWORK:DISTRIBUTION:LINKAGE objects
              DO nodeCount=1, TotAFNDistNodeObjs
                CALL GetObjectItem('AIRFLOWNETWORK:DISTRIBUTION:NODE',nodeCount,Alphas,NumAlphas,Numbers,NumNumbers,Status)
                IF (SameString(TRIM(Alphas(2)), TRIM(ATSDUNodeNames(atCount)))) THEN
                  MatchingATSDUAirFlowNodeNames(atCount) = Alphas(1)
                  EXIT
                END IF
              ENDDO
            END IF
          ENDDO

 

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!                                                       P R O C E S S I N G                                                        !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


          CALL DisplayString('Processing IDF -- Processing idf objects . . .')
          DO Num=1,NumIDFRecords

            IF (DeleteThisRecord(Num)) CYCLE
            DO xcount=IDFRecords(Num)%CommtS+1,IDFRecords(Num)%CommtE
              WRITE(DifLfn,fmta) TRIM(Comments(xcount))
              if (xcount == IDFRecords(Num)%CommtE) WRITE(DifLfn,fmta) ''
            ENDDO
            IF (NoVersion .and. Num == 1) THEN
              CALL GetNewObjectDefInIDD('VERSION',NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
              OutArgs(1) = sVersionNum
              CurArgs=1
              CALL WriteOutIDFLinesAsComments(DifLfn,'Version',CurArgs,OutArgs,NwFldNames,NwFldUnits)
            ENDIF

     ! deleted objects.  no transition.
            IF (MakeUPPERCase(TRIM(IDFRecords(Num)%Name)) == 'PROGRAMCONTROL') CYCLE
            IF (MakeUPPERCase(TRIM(IDFRecords(Num)%Name)) == 'SKY RADIANCE DISTRIBUTION') CYCLE
            IF (MakeUPPERCase(TRIM(IDFRecords(Num)%Name)) == 'AIRFLOW MODEL') CYCLE
            IF (MakeUPPERCase(TRIM(IDFRecords(Num)%Name)) == 'GENERATOR:FC:BATTERY DATA') CYCLE
            IF (MakeUPPERCase(TRIM(IDFRecords(Num)%Name)) == 'AIRFLOWNETWORK:MULTIZONE:SITEWINDCONDITIONS') CYCLE
            IF (MakeUPPERCase(TRIM(IDFRecords(Num)%Name)) == 'WATER HEATER:SIMPLE') THEN
              WRITE(DifLfn,fmta) '! ** The WATER HEATER:SIMPLE object has been deleted'
              CALL writePreprocessorObject(DifLfn,PrognameConversion,'Warning','The WATER HEATER:SIMPLE object has been deleted')
              CYCLE
            ENDIF

            ObjectName=IDFRecords(Num)%Name
            IF (FindItemInList(ObjectName,ObjectDef%Name,NumObjectDefs) /= 0) THEN
              CALL GetObjectDefInIDD(ObjectName,NumArgs,AorN,ReqFld,ObjMinFlds,FldNames,FldDefaults,FldUnits)
              NumAlphas=IDFRecords(Num)%NumAlphas
              NumNumbers=IDFRecords(Num)%NumNumbers
              Alphas(1:NumAlphas)=IDFRecords(Num)%Alphas(1:NumAlphas)
              Numbers(1:NumNumbers)=IDFRecords(Num)%Numbers(1:NumNumbers)
              CurArgs=NumAlphas+NumNumbers
              InArgs=Blank
              OutArgs=Blank
              TempArgs=Blank
              NA=0
              NN=0
              DO Arg=1,CurArgs
                IF (AorN(Arg)) THEN
                  NA=NA+1
                  InArgs(Arg)=Alphas(NA)
                ELSE
                  NN=NN+1
                  InArgs(Arg)=Numbers(NN)
                ENDIF
              ENDDO
            ELSE
              WRITE(Auditf,fmta) 'Object="'//TRIM(ObjectName)//'" does not seem to be on the "old" IDD.'
              WRITE(Auditf,fmta) '... will be listed as comments (no field names) on the new output file.'
              WRITE(Auditf,fmta) '... Alpha fields will be listed first, then numerics.'
              NumAlphas=IDFRecords(Num)%NumAlphas
              NumNumbers=IDFRecords(Num)%NumNumbers
              Alphas(1:NumAlphas)=IDFRecords(Num)%Alphas(1:NumAlphas)
              Numbers(1:NumNumbers)=IDFRecords(Num)%Numbers(1:NumNumbers)
              DO Arg=1,NumAlphas
                OutArgs(Arg)=Alphas(Arg)
              ENDDO
              NN=NumAlphas+1
              DO Arg=1,NumNumbers
                OutArgs(NN)=Numbers(Arg)
                NN=NN+1
              ENDDO
              CurArgs=NumAlphas+NumNumbers
              NwFldNames=Blank
              NwFldUnits=Blank
              CALL WriteOutIDFLinesAsComments(DifLfn,ObjectName,CurArgs,OutArgs,NwFldNames,NwFldUnits)
              CYCLE
            ENDIF

            Nodiff=.true.       ! Nodiff is true by default
            DiffMinFields=.false.
            Written=.false.

            IF (FindItemInList(MakeUPPERCase(ObjectName),NotInNew,SIZE(NotInNew)) == 0) THEN
              CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
              ! Check minfields
              IF (ObjMinFlds /= NwObjMinFlds) THEN
                DiffMinFields=.true.
              ELSE
                DiffMinFields=.false.
              ENDIF
            ENDIF

            IF (.not. MakingPretty) THEN

              SELECT CASE (MakeUPPERCase(TRIM(IDFRecords(Num)%Name)))

              CASE ('VERSION')
!                IF ((InArgs(1)(1:3)) == sVersionNum .and. ArgFile) THEN
!                  CALL ShowWarningError('File is already at latest version.  No new diff file made.',Auditf)
!                  CLOSE(diflfn,STATUS='DELETE')
!                  LatestVersion=.true.
!                  EXIT
!                ENDIF
!                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1) = sVersionNum
                NoDiff=.false.

    ! changes for this version, pick one of the spots to add rules, this will reduce the possibility of merge conflicts

!             CASE('OBJECTNAMEHERE')
!                 CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
!                 nodiff=.false.
!                 OutArgs(1)=InArgs(1)
!                 OutArgs(2) = 'SequentialLoad'
!                 OutArgs(3:CurArgs+1)=InArgs(2:CurArgs)
!                 CurArgs = CurArgs + 1
!                 NoDiff = .false.

              ! If your original object starts with A, insert the rules here
              CASE('AIRTERMINAL:SINGLEDUCT:UNCONTROLLED')
                 ! ZoneHVAC:EquipmentList already has a transition rule of its own, so add this object type change there
                 CALL GetNewObjectDefInIDD('AirTerminal:SingleDuct:ConstantVolume:NoReheat',NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                 nodiff=.false.
                 OutArgs(1:2)=InArgs(1:2)
                 OutArgs(3) = TRIM(InArgs(3)) // ' ATInlet'
                 OutArgs(4) =  InArgs(3)
                 OutArgs(5:7)=InArgs(4:6)

                CALL WriteOutIDFLines(DifLfn,'AirTerminal:SingleDuct:ConstantVolume:NoReheat',7,OutArgs,NwFldNames,NwFldUnits)
                NoDiff=.false.
                Written=.true.

                CALL GetNewObjectDefInIDD('ZoneHVAC:AirDistributionUnit',PNumArgs,PAOrN,PReqFld,PObjMinFlds,PFldNames,PFldDefaults,PFldUnits)
                POutArgs(1) = TRIM(InArgs(1)) // ' ADU'
                POutArgs(2) = InArgs(3)
                POutArgs(3) = 'AirTerminal:SingleDuct:ConstantVolume:NoReheat'
                POutArgs(4) = InArgs(1)
                POutArgs(5) = Blank
                POutArgs(6) = Blank
                POutArgs(7) = InArgs(7)
                CALL WriteOutIDFLines(DifLfn,'ZoneHVAC:AirDistributionUnit',PNumArgs,POutArgs,PFldNames,PFldUnits)

              ! This is part of the transition for AirTerminal:SingleDuct:Uncontrolled
              CASE('AIRLOOPHVAC:ZONESPLITTER')
              IF(TotATSDUObjs > 0) THEN
                 CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                 OutArgs(1:2) = InArgs(1:2)
                 ! Loop through outlet node names looking for a match to an AirTerminal:SingleDuct:Uncontrolled node name
                 DO nodeCountA=3, CurArgs
                   nodeFound = .false.
                   ! Is the node referenced directly here?
                   DO atCount=1, TotATSDUObjs
                     IF (SameString(TRIM(InArgs(nodeCountA)), TRIM(ATSDUNodeNames(atCount)))) THEN
                       nodeFound = .true.
                       EXIT
                     END IF
                   ENDDO
                   IF (.not. nodeFound) THEN
                     ! Is the node referenced via a NodeList here?
                     TotNodeListOjbs = GetNumObjectsFound('NODELIST')
                     DO nodeListCount=1, TotNodeListOjbs
                       CALL GetObjectItem('NODELIST',nodeListCount,Alphas,NumAlphas,Numbers,NumNumbers,Status)
                       ! Does this NodeList name match the AirloopHVAC field?
                       IF (SameString(TRIM(InArgs(nodeCountA)), TRIM(Alphas(1)))) THEN
                         DO nodeCount=2, CurArgs
                           DO atCount=1, TotATSDUObjs
                             IF (SameString(TRIM(Alphas(nodeCount)), TRIM(ATSDUNodeNames(atCount)))) THEN
                               nodeFound = .true.
                               EXIT
                             END IF
                           ENDDO
                           IF (nodeFound) EXIT
                         ENDDO
                       ELSE
                         CYCLE
                       END IF
                       IF (nodeFound) THEN
                         ! Make a new nodelist object
                         POutArgs(1) = TRIM(Alphas(1)) // ' ATInlet'
                         IF (nodeCount > 2) THEN
                           POutArgs(2:nodeCount-1) = Alphas(2:nodeCount-1)
                         END IF
                         POutArgs(nodeCount) = TRIM(Alphas(nodeCount)) // ' ATInlet'
                         IF (NumAlphas > nodeCount) THEN
                           POutArgs(nodeCount+1:NumAlphas) = Alphas(nodeCount+1:NumAlphas)
                         END IF
                         CALL GetNewObjectDefInIDD('NodeList',PNumArgs,PAOrN,PReqFld,PObjMinFlds,PFldNames,PFldDefaults,PFldUnits)
                         CALL WriteOutIDFLines(DifLfn,'NodeList',NumAlphas,POutArgs,PFldNames,PFldUnits)
                         EXIT
                       END IF
                     ENDDO
                   END IF
                   IF (nodeFound) THEN
                     OutArgs(nodeCountA) = TRIM(InArgs(nodeCountA)) // ' ATInlet'
                   ELSE
                     OutArgs(nodeCountA) = InArgs(nodeCountA)
                   END IF
                 ENDDO
                NoDiff = .false.
              ELSE
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                NoDiff=.true.
              ENDIF
                
              ! This is still part of the transition for AirTerminal:SingleDuct:Uncontrolled
              CASE('AIRLOOPHVAC:SUPPLYPLENUM')
              IF(TotATSDUObjs > 0) THEN
                 CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                 OutArgs(1:4) = InArgs(1:4)
                 ! Loop through outlet node names looking for a match to an AirTerminal:SingleDuct:Uncontrolled node name
                 DO nodeCountA=5, CurArgs
                   nodeFound = .false.
                   ! Is the node referenced directly here?
                   DO atCount=1, TotATSDUObjs
                     IF (SameString(TRIM(InArgs(nodeCountA)), TRIM(ATSDUNodeNames(atCount)))) THEN
                       nodeFound = .true.
                       EXIT
                     END IF
                   ENDDO
                   IF (.not. nodeFound) THEN
                     ! Is the node referenced via a NodeList here?
                     TotNodeListOjbs = GetNumObjectsFound('NODELIST')
                     DO nodeListCount=1, TotNodeListOjbs
                       CALL GetObjectItem('NODELIST',nodeListCount,Alphas,NumAlphas,Numbers,NumNumbers,Status)
                       ! Does this NodeList name match the AirloopHVAC field?
                       IF (SameString(TRIM(InArgs(nodeCountA)), TRIM(Alphas(1)))) THEN
                         DO nodeCount=2, CurArgs
                           DO atCount=1, TotATSDUObjs
                             IF (SameString(TRIM(Alphas(nodeCount)), TRIM(ATSDUNodeNames(atCount)))) THEN
                               nodeFound = .true.
                               EXIT
                             END IF
                           ENDDO
                           IF (nodeFound) EXIT
                         ENDDO
                       ELSE
                         CYCLE
                       END IF
                       IF (nodeFound) THEN
                         ! Make a new nodelist object
                         POutArgs(1) = TRIM(Alphas(1)) // ' ATInlet'
                         IF (nodeCount > 2) THEN
                           POutArgs(2:nodeCount-1) = Alphas(2:nodeCount-1)
                         END IF
                         POutArgs(nodeCount) = TRIM(Alphas(nodeCount)) // ' ATInlet'
                         IF (NumAlphas > nodeCount) THEN
                           POutArgs(nodeCount+1:NumAlphas) = Alphas(nodeCount+1:NumAlphas)
                         END IF
                         CALL GetNewObjectDefInIDD('NodeList',PNumArgs,PAOrN,PReqFld,PObjMinFlds,PFldNames,PFldDefaults,PFldUnits)
                         CALL WriteOutIDFLines(DifLfn,'NodeList',NumAlphas,POutArgs,PFldNames,PFldUnits)
                         EXIT
                       END IF
                     ENDDO
                   END IF
                   IF (nodeFound) THEN
                     OutArgs(nodeCountA) = TRIM(InArgs(nodeCountA)) // ' ATInlet'
                   ELSE
                     OutArgs(nodeCountA) = InArgs(nodeCountA)
                   END IF
                 ENDDO
                NoDiff = .false.
              ELSE
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                NoDiff=.true.
              ENDIF
                
              ! This is yet another part of the transition for AirTerminal:SingleDuct:Uncontrolled
              ! The node name could be referenced in an AirLoopHVAC F9 "Demand Side Inlet Node Names" either directly or in a NodeList
              CASE('AIRLOOPHVAC')
              IF(TotATSDUObjs > 0) THEN
                 CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                 nodeFound = .false.
                 ! Is the node referenced directly here?
                 DO atCount=1, TotATSDUObjs
                   IF (SameString(TRIM(InArgs(9)), TRIM(ATSDUNodeNames(atCount)))) THEN
                     nodeFound = .true.
                     EXIT
                   END IF
                 ENDDO
                 IF (.not. nodeFound) THEN
                   ! Is the node referenced via a NodeList here?
                   TotNodeListOjbs = GetNumObjectsFound('NODELIST')
                   DO nodeListCount=1, TotNodeListOjbs
                     CALL GetObjectItem('NODELIST',nodeListCount,Alphas,NumAlphas,Numbers,NumNumbers,Status)
                     ! Does this NodeList name match the AirloopHVAC field?
                     IF (SameString(TRIM(InArgs(9)), TRIM(Alphas(1)))) THEN
                       DO nodeCount=2, CurArgs
                         DO atCount=1, TotATSDUObjs
                           IF (SameString(TRIM(Alphas(nodeCount)), TRIM(ATSDUNodeNames(atCount)))) THEN
                             nodeFound = .true.
                             EXIT
                           END IF
                         ENDDO
                         IF (nodeFound) EXIT
                       ENDDO
                     ELSE
                       CYCLE
                     END IF
                     IF (nodeFound) THEN
                       ! Make a new nodelist object
                       POutArgs(1) = TRIM(Alphas(1)) // ' ATInlet'
                       IF (nodeCount > 2) THEN
                         POutArgs(2:nodeCount-1) = Alphas(2:nodeCount-1)
                       END IF
                       POutArgs(nodeCount) = TRIM(Alphas(nodeCount)) // ' ATInlet'
                       IF (NumAlphas > nodeCount) THEN
                         POutArgs(nodeCount+1:NumAlphas) = Alphas(nodeCount+1:NumAlphas)
                       END IF
                       CALL GetNewObjectDefInIDD('NodeList',PNumArgs,PAOrN,PReqFld,PObjMinFlds,PFldNames,PFldDefaults,PFldUnits)
                       CALL WriteOutIDFLines(DifLfn,'NodeList',NumAlphas,POutArgs,PFldNames,PFldUnits)
                       EXIT
                     END IF
                   ENDDO
                 END IF

                 ! Now, finally, modify the AirLoopHVAC object, or pass it through
                 IF (nodeFound) THEN
                   ! Found the node name directly referenced or via a NodeList
                   NoDiff = .false.
                   OutArgs(1:8) = InArgs(1:8)
                   OutArgs(9) = TRIM(InArgs(9)) // ' ATInlet'
                   OutArgs(10:CurArgs) = InArgs(10:CurArgs)
                 ELSE
                   ! Node name not found - past this object through untouched
                   OutArgs(1:CurArgs) = InArgs(1:CurArgs)
                   NoDiff = .true.
                 END IF
              ELSE
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                NoDiff=.true.
              ENDIF
                
              ! Wait - there's more for the transition for AirTerminal:SingleDuct:Uncontrolled
              CASE('ROOMAIR:NODE:AIRFLOWNETWORK:HVACEQUIPMENT')
              IF(TotATSDUObjs > 0) THEN
                 CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                 OutArgs(1) = InArgs(1)
                 ! Loop through fields looking for  AirTerminal:SingleDuct:Uncontrolled node name
                 DO nodeCount=2, CurArgs
                   IF (SameString(TRIM(InArgs(nodeCount)), TRIM(ATSDUNodeNames(atCount)))) THEN
                     OutArgs(nodeCount) = TRIM(InArgs(nodeCount)) // ' ATInlet'
                   ELSE
                     OutArgs(nodeCount) = InArgs(nodeCount)
                   END IF
                 ENDDO
                NoDiff = .false.
              ELSE
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                NoDiff=.true.
              ENDIF
                
              ! And even more for the transition for AirTerminal:SingleDuct:Uncontrolled
              CASE('AIRFLOWNETWORK:DISTRIBUTION:NODE')
              IF(TotATSDUObjs > 0) THEN
                 CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                 ! Does this node match an AirTerminal:SingleDuct:Uncontrolled node name
                 nodeFound = .false.
                 DO atCount=1, TotATSDUObjs
                   IF (SameString(TRIM(InArgs(2)), TRIM(ATSDUNodeNames(atCount)))) THEN
                     nodeFound = .true.
                     EXIT
                   END IF
                 ENDDO
                 IF (nodeFound) THEN
                   ! Create companion object for the new node name
                   POutArgs(1) = TRIM(InArgs(1)) // ' ATInlet'
                   POutArgs(2) = TRIM(InArgs(2)) // ' ATInlet'
                   IF (CurArgs > 2) POutArgs(3:CurArgs) = InArgs(3:CurArgs)
                   CALL WriteOutIDFLines(DifLfn,'AirflowNetwork:Distribution:Node',CurArgs,POutArgs,NwFldNames,NwFldUnits)
                 END IF

                 ! Either way, do nothing with this object, let it pass through unchanged
                 OutArgs(1:CurArgs) = InArgs(1:CurArgs)
                 NoDiff = .true.
              ELSE
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                NoDiff=.true.
              ENDIF

              ! And still more for the transition for AirTerminal:SingleDuct:Uncontrolled
              CASE('AIRFLOWNETWORK:DISTRIBUTION:LINKAGE')
              IF(TotATSDUObjs > 0) THEN
                 CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                 ! Does this linkage contain an match an AIRFLOWNETWORK:DISTRIBUTION:NODE that matches an AirTerminal:SingleDuct:Uncontrolled node name
                 nodeFound = .false.
                 DO atCount=1, TotATSDUObjs
                   IF (SameString(TRIM(InArgs(3)), TRIM(MatchingATSDUAirFlowNodeNames(atCount)))) THEN
                     nodeFound = .true.
                     EXIT
                   END IF
                 ENDDO
                 IF (nodeFound) THEN
                   ! Modify this object
                   OutArgs(1:2) = InArgs(1:2)
                   OutArgs(3) = TRIM(InArgs(3)) // ' ATInlet'
                   OutArgs(4) = InArgs(4) ! this is required, so it should be present
                   NoDiff = .false.
                   IF (CurArgs > 4) OutArgs(5) = InArgs(5)
                   ! Create companion object for the new linkage from InArgs3...ATInlet to original outlet INArgs3
                   POutArgs(1) = TRIM(InArgs(1)) // ' ATInlet'
                   POutArgs(2) = TRIM(InArgs(3)) // ' ATInlet'
                   POutArgs(3) = InArgs(3)
                   POutArgs(4) = InArgs(4) ! this is required, so it should be present
                   IF (CurArgs > 4) POutArgs(5) = InArgs(5)
                   CALL WriteOutIDFLines(DifLfn,'AirflowNetwork:Distribution:Linkage',CurArgs,POutArgs,NwFldNames,NwFldUnits)
                 ELSE
                   ! Do nothing with this object, let it pass through unchanged
                   OutArgs(1:CurArgs) = InArgs(1:CurArgs)
                   NoDiff = .true.
                 END IF
              ELSE
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                NoDiff=.true.
              ENDIF
              ! Whew - Done with the transition for AirTerminal:SingleDuct:Uncontrolled
              ! Well - almost done, there's more in the case below for ZoneHVAC:EquipmentList . . .

              ! If your original object starts with B, insert the rules here

              ! If your original object starts with C, insert the rules here

              ! If your original object starts with D, insert the rules here

              ! If your original object starts with E, insert the rules here

              ! If your original object starts with F, insert the rules here

              ! If your original object starts with G, insert the rules here

              ! If your original object starts with H, insert the rules here

              ! If your original object starts with I, insert the rules here

              ! If your original object starts with L, insert the rules here

              ! If your original object starts with M, insert the rules here

              ! If your original object starts with N, insert the rules here

              ! If your original object starts with O, insert the rules here

              ! If your original object starts with P, insert the rules here

              ! If your original object starts with R, insert the rules here

              ! If your original object starts with T, insert the rules here

              ! If your original object starts with U, insert the rules here

              ! If your original object starts with V, insert the rules here

              ! If your original object starts with W, insert the rules here

              ! If your original object starts with Z, insert the rules here
              CASE('ZONEHVAC:EQUIPMENTLIST')
                nodiff = .false.
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                DO CurField = 1, CurArgs
                    ! Add air terminal unit object type and name change here (see AirTerminal:SingleDuct:Uncontrolled above)
                    IF (CurField > 2) THEN
                      IF (SameString(TRIM(InArgs(CurField)),'AirTerminal:SingleDuct:Uncontrolled')) THEN
                        OutArgs(CurField) = 'ZoneHVAC:AirDistributionUnit'
                      ELSE IF (SameString(TRIM(InArgs(CurField-1)),'AirTerminal:SingleDuct:Uncontrolled')) THEN
                        OutArgs(CurField) = TRIM(InArgs(CurField)) // ' ADU'
                      ELSE
                        OutArgs(CurField) = InArgs(CurField)
                      END IF
                    ELSE
                      OutArgs(CurField) = InArgs(CurField)
                    END IF
                END DO

    !!!   Changes for report variables, meters, tables -- update names
              CASE('OUTPUT:VARIABLE')
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                nodiff=.true.
                IF (OutArgs(1) == Blank) THEN
                  OutArgs(1)='*'
                  nodiff=.false.
                ENDIF

                CALL ScanOutputVariablesForReplacement(  &
                   2,  &
                   DelThis,  &
                   checkrvi,  &
                   nodiff,  &
                   ObjectName,  &
                   DifLfn,      &
                   .true.,  & !OutVar
                   .false., & !MtrVar
                   .false., & !TimeBinVar
                   CurArgs, &
                   Written, &
                   .false.)
                IF (DelThis) CYCLE

              CASE ('OUTPUT:METER','OUTPUT:METER:METERFILEONLY','OUTPUT:METER:CUMULATIVE','OUTPUT:METER:CUMULATIVE:METERFILEONLY')
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                nodiff=.true.
                CALL ScanOutputVariablesForReplacement(  &
                   1,  &
                   DelThis,  &
                   checkrvi,  &
                   nodiff,  &
                   ObjectName,  &
                   DifLfn,      &
                   .false.,  & !OutVar
                   .true., & !MtrVar
                   .false., & !TimeBinVar
                   CurArgs, &
                   Written, &
                   .false.)
                IF (DelThis) CYCLE

              CASE('OUTPUT:TABLE:TIMEBINS')
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                nodiff=.true.
                IF (OutArgs(1) == Blank) THEN
                  OutArgs(1)='*'
                  nodiff=.false.
                ENDIF
                CALL ScanOutputVariablesForReplacement(  &
                   2,  &
                   DelThis,  &
                   checkrvi,  &
                   nodiff,  &
                   ObjectName,  &
                   DifLfn,      &
                   .false.,  & !OutVar
                   .false., & !MtrVar
                   .true., & !TimeBinVar
                   CurArgs, &
                   Written, &
                   .false.)
                IF (DelThis) CYCLE

!ExternalInterface:FunctionalMockupUnitImport:From:Variable, field 2
!ExternalInterface:FunctionalMockupUnitExport:From:Variable, field 2
              CASE('EXTERNALINTERFACE:FUNCTIONALMOCKUPUNITIMPORT:FROM:VARIABLE',  &
                   'EXTERNALINTERFACE:FUNCTIONALMOCKUPUNITEXPORT:FROM:VARIABLE')
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                nodiff=.true.
                IF (OutArgs(1) == Blank) THEN
                  OutArgs(1)='*'
                  nodiff=.false.
                ENDIF
                CALL ScanOutputVariablesForReplacement(  &
                   2,  &
                   DelThis,  &
                   checkrvi,  &
                   nodiff,  &
                   ObjectName,  &
                   DifLfn,      &
                   .false.,  & !OutVar
                   .false., & !MtrVar
                   .false., & !TimeBinVar
                   CurArgs, &
                   Written, &
                   .false.)
                IF (DelThis) CYCLE

!EnergyManagementSystem:Sensor, field 3
              CASE('ENERGYMANAGEMENTSYSTEM:SENSOR')
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                nodiff=.true.
                CALL ScanOutputVariablesForReplacement(  &
                   3,  &
                   DelThis,  &
                   checkrvi,  &
                   nodiff,  &
                   ObjectName,  &
                   DifLfn,      &
                   .false.,  & !OutVar
                   .false., & !MtrVar
                   .false., & !TimeBinVar
                   CurArgs, &
                   Written, &
                   .true.)
                IF (DelThis) CYCLE

              CASE('OUTPUT:TABLE:MONTHLY')
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                nodiff=.true.
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                CurVar=3
                DO Var=3,CurArgs,2
                  UCRepVarName=MakeUPPERCase(InArgs(Var))
                  OutArgs(CurVar)=InArgs(Var)
                  OutArgs(CurVar+1)=InArgs(Var+1)
                  pos=INDEX(UCRepVarName,'[')
                  IF (pos > 0) THEN
                    UCRepVarName=UCRepVarName(1:pos-1)
                    OutArgs(CurVar)=InArgs(Var)(1:pos-1)
                    OutArgs(CurVar+1)=InArgs(Var+1)
                  ENDIF
                  DelThis=.false.
                  DO Arg=1,NumRepVarNames
                    UCCompRepVarName=MakeUPPERCase(OldRepVarName(Arg))
                    IF (UCCompRepVarName(Len_Trim(UCCompRepVarName):Len_Trim(UCCompRepVarName)) == '*') THEN
                      WildMatch=.true.
                      UCCompRepVarName(Len_Trim(UCCompRepVarName):Len_Trim(UCCompRepVarName))=' '
                      pos=INDEX(TRIM(UCRepVarname),TRIM(UCCompRepVarName))
                    ELSE
                      WildMatch=.false.
                      pos=0
                      if (UCRepVarName == UCCompRepVarName) pos=1
                    ENDIF
                    IF (pos > 0 .and. pos /= 1) CYCLE
                    IF (pos > 0) THEN
                      IF (NewRepVarName(Arg) /= '<DELETE>') THEN
                        IF (.not. WildMatch) THEN
                          OutArgs(CurVar)=NewRepVarName(Arg)
                        ELSE
                          OutArgs(CurVar)=TRIM(NewRepVarName(Arg))//OutArgs(CurVar)(Len_Trim(UCCompRepVarName)+1:)
                        ENDIF
                        IF (NewRepVarCaution(Arg) /= Blank .and. .not. SameString(NewRepVarCaution(Arg)(1:6),'Forkeq') ) THEN
                          IF (.not. OTMVarCaution(Arg)) THEN  ! caution message not written yet
                            CALL writePreprocessorObject(DifLfn,PrognameConversion,'Warning',  &
                               'Output Table Monthly (old)="'//trim(OldRepVarName(Arg))//  &
                               '" conversion to Output Table Monthly (new)="'//  &
                               trim(NewRepVarName(Arg))//'" has the following caution "'//trim(NewRepVarCaution(Arg))//'".')
                            write(diflfn,fmtA) ' '
                            OTMVarCaution(Arg)=.true.
                          ENDIF
                        ENDIF
                        OutArgs(CurVar+1)=InArgs(Var+1)
                        nodiff=.false.
                      ELSE
                        DelThis=.true.
                      ENDIF
                      IF (OldRepVarName(Arg) == OldRepVarName(Arg+1)) THEN
                        IF (.not. SameString(NewRepVarCaution(Arg)(1:6),'Forkeq')) THEN
                          ! Adding a var field.
                          CurVar=CurVar+2
                          IF (.not. WildMatch) THEN
                            OutArgs(CurVar)=NewRepVarName(Arg+1)
                          ELSE
                            OutArgs(CurVar)=TRIM(NewRepVarName(Arg+1))//OutArgs(CurVar)(Len_Trim(UCCompRepVarName)+1:)
                          ENDIF
                          IF (NewRepVarCaution(Arg+1) /= Blank) THEN
                            IF (.not. OTMVarCaution(Arg+1)) THEN  ! caution message not written yet
                              CALL writePreprocessorObject(DifLfn,PrognameConversion,'Warning',  &
                                 'Output Table Monthly (old)="'//trim(OldRepVarName(Arg))//  &
                                 '" conversion to Output Table Monthly (new)="'//  &
                                 trim(NewRepVarName(Arg+1))//'" has the following caution "'//trim(NewRepVarCaution(Arg+1))//'".')
                              write(diflfn,fmtA) ' '
                              OTMVarCaution(Arg+1)=.true.
                            ENDIF
                          ENDIF
                          OutArgs(CurVar+1)=InArgs(Var+1)
                          nodiff=.false.
                        ENDIF
                      ENDIF
                      IF (OldRepVarName(Arg) == OldRepVarName(Arg+2)) THEN  ! only 1 more... for ForkEq
                        ! Adding a var field.
                        CurVar=CurVar+2
                        IF (.not. WildMatch) THEN
                          OutArgs(CurVar)=NewRepVarName(Arg+2)
                        ELSE
                          OutArgs(CurVar)=TRIM(NewRepVarName(Arg+2))//OutArgs(CurVar)(Len_Trim(UCCompRepVarName)+1:)
                        ENDIF
                        IF (NewRepVarCaution(Arg+2) /= Blank) THEN
                          IF (.not. OTMVarCaution(Arg+2)) THEN  ! caution message not written yet
                            CALL writePreprocessorObject(DifLfn,PrognameConversion,'Warning',  &
                               'Output Table Monthly (old)="'//trim(OldRepVarName(Arg))//  &
                               '" conversion to Output Table Monthly (new)="'//  &
                               trim(NewRepVarName(Arg+2))//'" has the following caution "'//trim(NewRepVarCaution(Arg+2))//'".')
                            write(diflfn,fmtA) ' '
                            OTMVarCaution(Arg+2)=.true.
                          ENDIF
                        ENDIF
                        OutArgs(CurVar+1)=InArgs(Var+1)
                        nodiff=.false.
                      ENDIF
                      EXIT
                    ENDIF
                  ENDDO
                  IF (.not. DelThis) CurVar=CurVar+2
                ENDDO
                CurArgs=CurVar-1

              CASE('METER:CUSTOM')
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                nodiff=.true.
                CurVar=4
                DO Var=4,CurArgs,2
                  UCRepVarName=MakeUPPERCase(InArgs(Var))
                  OutArgs(CurVar)=InArgs(Var)
                  OutArgs(CurVar+1)=InArgs(Var+1)
                  pos=INDEX(UCRepVarName,'[')
                  IF (pos > 0) THEN
                    UCRepVarName=UCRepVarName(1:pos-1)
                    OutArgs(CurVar)=InArgs(Var)(1:pos-1)
                    OutArgs(CurVar+1)=InArgs(Var+1)
                  ENDIF
                  DelThis=.false.
                  DO Arg=1,NumRepVarNames
                    UCCompRepVarName=MakeUPPERCase(OldRepVarName(Arg))
                    IF (UCCompRepVarName(Len_Trim(UCCompRepVarName):Len_Trim(UCCompRepVarName)) == '*') THEN
                      WildMatch=.true.
                      UCCompRepVarName(Len_Trim(UCCompRepVarName):Len_Trim(UCCompRepVarName))=' '
                      pos=INDEX(TRIM(UCRepVarname),TRIM(UCCompRepVarName))
                    ELSE
                      WildMatch=.false.
                      pos=0
                      if (UCRepVarName == UCCompRepVarName) pos=1
                    ENDIF
                    IF (pos > 0 .and. pos /= 1) CYCLE
                    IF (pos > 0) THEN
                      IF (NewRepVarName(Arg) /= '<DELETE>') THEN
                        IF (.not. WildMatch) THEN
                          OutArgs(CurVar)=NewRepVarName(Arg)
                        ELSE
                          OutArgs(CurVar)=TRIM(NewRepVarName(Arg))//OutArgs(CurVar)(Len_Trim(UCCompRepVarName)+1:)
                        ENDIF
                        IF (NewRepVarCaution(Arg) /= Blank .and. .not. SameString(NewRepVarCaution(Arg)(1:6),'Forkeq') ) THEN
                          IF (.not. CMtrVarCaution(Arg)) THEN  ! caution message not written yet
                            CALL writePreprocessorObject(DifLfn,PrognameConversion,'Warning',  &
                               'Custom Meter (old)="'//trim(OldRepVarName(Arg))//  &
                               '" conversion to Custom Meter (new)="'//  &
                               trim(NewRepVarName(Arg))//'" has the following caution "'//trim(NewRepVarCaution(Arg))//'".')
                            write(diflfn,fmtA) ' '
                            CMtrVarCaution(Arg)=.true.
                          ENDIF
                        ENDIF
                        OutArgs(CurVar+1)=InArgs(Var+1)
                        nodiff=.false.
                      ELSE
                        DelThis=.true.
                      ENDIF
                      IF (OldRepVarName(Arg) == OldRepVarName(Arg+1)) THEN
                        IF (.not. SameString(NewRepVarCaution(Arg)(1:6),'Forkeq')) THEN
                          ! Adding a var field.
                          CurVar=CurVar+2
                          IF (.not. WildMatch) THEN
                            OutArgs(CurVar)=NewRepVarName(Arg+1)
                          ELSE
                            OutArgs(CurVar)=TRIM(NewRepVarName(Arg+1))//OutArgs(CurVar)(Len_Trim(UCCompRepVarName)+1:)
                          ENDIF
                          IF (NewRepVarCaution(Arg+1) /= Blank .and. .not. SameString(NewRepVarCaution(Arg+1)(1:6),'Forkeq') ) THEN
                            IF (.not. CMtrVarCaution(Arg+1)) THEN  ! caution message not written yet
                              CALL writePreprocessorObject(DifLfn,PrognameConversion,'Warning',  &
                                 'Custom Meter (old)="'//trim(OldRepVarName(Arg))//  &
                                 '" conversion to Custom Meter (new)="'//  &
                                 trim(NewRepVarName(Arg+1))//'" has the following caution "'//trim(NewRepVarCaution(Arg+1))//'".')
                              write(diflfn,fmtA) ' '
                              CMtrVarCaution(Arg+1)=.true.
                            ENDIF
                          ENDIF
                          OutArgs(CurVar+1)=InArgs(Var+1)
                          nodiff=.false.
                        ENDIF
                      ENDIF
                      IF (OldRepVarName(Arg) == OldRepVarName(Arg+2)) THEN
                        ! Adding a var field.
                        CurVar=CurVar+2
                        IF (.not. WildMatch) THEN
                          OutArgs(CurVar)=NewRepVarName(Arg+2)
                        ELSE
                          OutArgs(CurVar)=TRIM(NewRepVarName(Arg+2))//OutArgs(CurVar)(Len_Trim(UCCompRepVarName)+1:)
                        ENDIF
                        IF (NewRepVarCaution(Arg+2) /= Blank) THEN
                          IF (.not. CMtrVarCaution(Arg+2)) THEN  ! caution message not written yet
                            CALL writePreprocessorObject(DifLfn,PrognameConversion,'Warning',  &
                               'Custom Meter (old)="'//trim(OldRepVarName(Arg))//  &
                               '" conversion to Custom Meter (new)="'//  &
                               trim(NewRepVarName(Arg+2))//'" has the following caution "'//trim(NewRepVarCaution(Arg+2))//'".')
                            write(diflfn,fmtA) ' '
                            CMtrVarCaution(Arg+2)=.true.
                          ENDIF
                        ENDIF
                        OutArgs(CurVar+1)=InArgs(Var+1)
                        nodiff=.false.
                      ENDIF
                      EXIT
                    ENDIF
                  ENDDO
                  IF (.not. DelThis) CurVar=CurVar+2
                ENDDO
                CurArgs=CurVar
                DO Arg=CurVar,1,-1
                  IF (OutArgs(Arg) == Blank) THEN
                    CurArgs=CurArgs-1
                  ELSE
                    EXIT
                  ENDIF
                ENDDO

              CASE('METER:CUSTOMDECREMENT')
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                nodiff=.true.
                CurVar=4   ! In case Source Meter would change
                DO Var=4,CurArgs,2
                  UCRepVarName=MakeUPPERCase(InArgs(Var))
                  OutArgs(CurVar)=InArgs(Var)
                  OutArgs(CurVar+1)=InArgs(Var+1)
                  pos=INDEX(UCRepVarName,'[')
                  IF (pos > 0) THEN
                    UCRepVarName=UCRepVarName(1:pos-1)
                    OutArgs(CurVar)=InArgs(Var)(1:pos-1)
                    OutArgs(CurVar+1)=InArgs(Var+1)
                  ENDIF
                  DelThis=.false.
                  DO Arg=1,NumRepVarNames
                    UCCompRepVarName=MakeUPPERCase(OldRepVarName(Arg))
                    IF (UCCompRepVarName(Len_Trim(UCCompRepVarName):Len_Trim(UCCompRepVarName)) == '*') THEN
                      WildMatch=.true.
                      UCCompRepVarName(Len_Trim(UCCompRepVarName):Len_Trim(UCCompRepVarName))=' '
                      pos=INDEX(TRIM(UCRepVarname),TRIM(UCCompRepVarName))
                    ELSE
                      WildMatch=.false.
                      pos=0
                      if (UCRepVarName == UCCompRepVarName) pos=1
                    ENDIF
                    IF (pos > 0 .and. pos /= 1) CYCLE
                    IF (pos > 0) THEN
                      IF (NewRepVarName(Arg) /= '<DELETE>') THEN
                        IF (.not. WildMatch) THEN
                          OutArgs(CurVar)=NewRepVarName(Arg)
                        ELSE
                          OutArgs(CurVar)=TRIM(NewRepVarName(Arg))//OutArgs(CurVar)(Len_Trim(UCCompRepVarName)+1:)
                        ENDIF
                        IF (NewRepVarCaution(Arg) /= Blank .and. .not. SameString(NewRepVarCaution(Arg)(1:6),'Forkeq') ) THEN
                          IF (.not. CMtrDVarCaution(Arg)) THEN  ! caution message not written yet
                            CALL writePreprocessorObject(DifLfn,PrognameConversion,'Warning',  &
                               'Custom Decrement Meter (old)="'//trim(OldRepVarName(Arg))//  &
                               '" conversion to Custom Meter (new)="'//  &
                               trim(NewRepVarName(Arg))//'" has the following caution "'//trim(NewRepVarCaution(Arg))//'".')
                            write(diflfn,fmtA) ' '
                            CMtrDVarCaution(Arg)=.true.
                          ENDIF
                        ENDIF
                        OutArgs(CurVar+1)=InArgs(Var+1)
                        nodiff=.false.
                      ELSE
                        DelThis=.true.
                      ENDIF
                      IF (OldRepVarName(Arg) == OldRepVarName(Arg+1)) THEN
                        IF (.not. SameString(NewRepVarCaution(Arg)(1:6),'Forkeq')) THEN
                          ! Adding a var field.
                          CurVar=CurVar+2
                          IF (.not. WildMatch) THEN
                            OutArgs(CurVar)=NewRepVarName(Arg+1)
                          ELSE
                            OutArgs(CurVar)=TRIM(NewRepVarName(Arg+1))//OutArgs(CurVar)(Len_Trim(UCCompRepVarName)+1:)
                          ENDIF
                          IF (NewRepVarCaution(Arg+1) /= Blank .and. .not. SameString(NewRepVarCaution(Arg+1)(1:6),'Forkeq') ) THEN
                            IF (.not. CMtrDVarCaution(Arg+1)) THEN  ! caution message not written yet
                              CALL writePreprocessorObject(DifLfn,PrognameConversion,'Warning',  &
                                 'Custom Decrement Meter (old)="'//trim(OldRepVarName(Arg))//  &
                                 '" conversion to Custom Decrement Meter (new)="'//  &
                                 trim(NewRepVarName(Arg+1))//'" has the following caution "'//trim(NewRepVarCaution(Arg+1))//'".')
                              write(diflfn,fmtA) ' '
                              CMtrDVarCaution(Arg+1)=.true.
                            ENDIF
                          ENDIF
                          OutArgs(CurVar+1)=InArgs(Var+1)
                          nodiff=.false.
                        ENDIF
                      ENDIF
                      IF (OldRepVarName(Arg) == OldRepVarName(Arg+2)) THEN
                        ! Adding a var field.
                        CurVar=CurVar+2
                        IF (.not. WildMatch) THEN
                          OutArgs(CurVar)=NewRepVarName(Arg+2)
                        ELSE
                          OutArgs(CurVar)=TRIM(NewRepVarName(Arg+2))//OutArgs(CurVar)(Len_Trim(UCCompRepVarName)+1:)
                        ENDIF
                        IF (NewRepVarCaution(Arg+2) /= Blank) THEN
                          IF (.not. CMtrDVarCaution(Arg+2)) THEN  ! caution message not written yet
                            CALL writePreprocessorObject(DifLfn,PrognameConversion,'Warning',  &
                               'Custom Decrement Meter (old)="'//trim(OldRepVarName(Arg))//  &
                               '" conversion to Custom Meter (new)="'//  &
                               trim(NewRepVarName(Arg+2))//'" has the following caution "'//trim(NewRepVarCaution(Arg+2))//'".')
                            write(diflfn,fmtA) ' '
                            CMtrDVarCaution(Arg+2)=.true.
                          ENDIF
                        ENDIF
                        OutArgs(CurVar+1)=InArgs(Var+1)
                        nodiff=.false.
                      ENDIF
                      EXIT
                    ENDIF
                  ENDDO
                  IF (.not. DelThis) CurVar=CurVar+2
                ENDDO
                CurArgs=CurVar
                DO Arg=CurVar,1,-1
                  IF (OutArgs(Arg) == Blank) THEN
                    CurArgs=CurArgs-1
                  ELSE
                    EXIT
                  ENDIF
                ENDDO

              CASE DEFAULT
                  IF (FindItemInList(ObjectName,NotInNew,SIZE(NotInNew)) /= 0) THEN
                    WRITE(Auditf,fmta) 'Object="'//TRIM(ObjectName)//'" is not in the "new" IDD.'
                    WRITE(Auditf,fmta) '... will be listed as comments on the new output file.'
                    CALL WriteOutIDFLinesAsComments(DifLfn,ObjectName,CurArgs,InArgs,FldNames,FldUnits)
                    Written=.true.
                    !CYCLE
                  ELSE
                    CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                    OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                    NoDiff=.true.
                  ENDIF

              END SELECT

            ELSE   !!! Making Pretty

              ! Just making pretty -- no changes as above.
              CALL GetNewObjectDefInIDD(IDFRecords(Num)%Name,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
              OutArgs(1:CurArgs)=InArgs(1:CurArgs)
            ENDIF

            IF (DiffMinFields .and. nodiff) THEN
              ! Change in min-fields
                CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
                OutArgs(1:CurArgs)=InArgs(1:CurArgs)
                NoDiff=.false.
                DO Arg=CurArgs+1,NwObjMinFlds
                  OutArgs(Arg)=NwFldDefaults(Arg)
                ENDDO
                CurArgs=MAX(NwObjMinFlds,CurArgs)
            ENDIF

            IF (NoDiff .and. DiffOnly) CYCLE

            !! reformat for better readability
            !! BUILDING,SOLUTION ALGORITHM,OUTSIDE CONVECTION ALGORITHM,INSIDE CONVECTION ALGORITHM,REPORT VARIABLE,
            !! SURFACE:HEATTRANSFER,SURFACE:HEATTRANSFER:SUBSURFACE:SHADING:DETACHED,
            !! SURFACE:SHADING:DETACHED:FIXED,SURFACE:SHADING:DETACHED:BUILDING,
            !! SURFACE:SHADING:ATTACHED,
            !! WINDOWGLASSSPECTRALDATA,
            !! FLUIDPROPERTYTEMPERATURES,
            !! FLUIDPROPERTYSATURATED,FLUIDPROPERTYSUPERHEATED,FLUIDPROPERTYCONCENTRATION
            IF (.not. Written) THEN
              CALL CheckSpecialObjects(DifLfn,ObjectName,CurArgs,OutArgs,NwFldNames,NwFldUnits,Written)
            ENDIF

            IF (.not. Written) THEN
              CALL WriteOutIDFLines(DifLfn,ObjectName,CurArgs,OutArgs,NwFldNames,NwFldUnits)
            ENDIF

          ENDDO  ! IDFRecords

          CALL DisplayString('Processing IDF -- Processing idf objects complete.')
          IF (IDFRecords(NumIDFRecords)%CommtE /= CurComment) THEN
            DO xcount=IDFRecords(NumIDFRecords)%CommtE+1,CurComment
              WRITE(DifLfn,fmta) TRIM(Comments(xcount))
              if (xcount == IDFRecords(Num)%CommtE) WRITE(DifLfn,fmta) ''
            ENDDO
          ENDIF

          IF (GetNumSectionsFound('Report Variable Dictionary') > 0) THEN
            ObjectName='Output:VariableDictionary'
            CALL GetNewObjectDefInIDD(ObjectName,NwNumArgs,NwAorN,NwReqFld,NwObjMinFlds,NwFldNames,NwFldDefaults,NwFldUnits)
            nodiff=.false.
            OutArgs(1)='Regular'
            CurArgs=1
            CALL WriteOutIDFLines(DifLfn,ObjectName,CurArgs,OutArgs,NwFldNames,NwFldUnits)
          ENDIF

          INQUIRE(FILE=trim(FileNamePath)//'.rvi',EXIST=FileExist)
!          IF (FileExist) THEN
!            CALL writePreprocessorObject(DifLfn,PrognameConversion,'Warning',  &
!               'rvi file associated with this input is being processed. Review for accuracy.')
!            write(diflfn,fmtA) ' '
!          ENDIF
          CLOSE(DifLfn)
          CALL ProcessRviMviFiles(FileNamePath,'rvi')
          CALL ProcessRviMviFiles(FileNamePath,'mvi')
          CALL CloseOut
        ELSE  ! not a idf or imf
          CALL ProcessRviMviFiles(FileNamePath,'rvi')
          CALL ProcessRviMviFiles(FileNamePath,'mvi')
        ENDIF
      ELSE  ! Full name == Blank
        EndOfFile=.true.
      ENDIF

      CALL CreateNewName('Reallocate',CreatedOutputName,' ')

    ENDDO

    IF (.not. ExitBecauseBadFile) THEN
      StillWorking=.false.
      EXIT
    ELSE
      IF (.not. ArgFileBeingDone) THEN
        EndOfFile=.false.
      ELSE
        EndOfFile=.true.
        StillWorking=.false.
      ENDIF
    ENDIF
  ENDDO

  IF (ArgFileBeingDone .and. .not. LatestVersion .and. .not. ExitBecauseBadFile) THEN
    ! If this is true, then there was a "arg IDF File" on the command line and some files need to be renamed
    ErrFlag=.false.
    CALL copyfile(TRIM(FileNamePath)//'.'//TRIM(ArgIDFExtension),TRIM(FileNamePath)//'.'//TRIM(ArgIDFExtension)//'old',ErrFlag)
    CALL copyfile(TRIM(FileNamePath)//'.'//TRIM(ArgIDFExtension)//'new',TRIM(FileNamePath)//'.'//TRIM(ArgIDFExtension),ErrFlag)
    INQUIRE(File=TRIM(FileNamePath)//'.rvi',EXIST=FileExist)
    IF (FileExist) THEN
      CALL copyfile(TRIM(FileNamePath)//'.rvi',TRIM(FileNamePath)//'.rviold',ErrFlag)
    ENDIF
    INQUIRE(File=TRIM(FileNamePath)//'.rvinew',EXIST=FileExist)
    IF (FileExist) THEN
      CALL copyfile(TRIM(FileNamePath)//'.rvinew',TRIM(FileNamePath)//'.rvi',ErrFlag)
    ENDIF
    INQUIRE(File=TRIM(FileNamePath)//'.mvi',EXIST=FileExist)
    IF (FileExist) THEN
      CALL copyfile(TRIM(FileNamePath)//'.mvi',TRIM(FileNamePath)//'.mviold',ErrFlag)
    ENDIF
    INQUIRE(File=TRIM(FileNamePath)//'.mvinew',EXIST=FileExist)
    IF (FileExist) THEN
      CALL copyfile(TRIM(FileNamePath)//'.mvinew',TRIM(FileNamePath)//'.mvi',ErrFlag)
    ENDIF
  ENDIF

  RETURN

END SUBROUTINE CreateNewIDFUsingRules
