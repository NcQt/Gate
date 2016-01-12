/*----------------------
  Copyright (C): OpenGATE Collaboration

  This software is distributed under the terms
  of the GNU Lesser General  Public Licence (LGPL)
  See GATE/LICENSE.txt for further details
  ----------------------*/


/*
  \brief Class GateLETActor :
  \brief
*/

// gate
#include "GateLETActor.hh"
#include "GateMiscFunctions.hh"

// g4
#include <G4EmCalculator.hh>
#include <G4VoxelLimits.hh>
#include <G4NistManager.hh>
#include <G4PhysicalConstants.hh>

//-----------------------------------------------------------------------------
GateLETActor::GateLETActor(G4String name, G4int depth):
  GateVImageActor(name,depth) {
  GateDebugMessageInc("Actor",4,"GateLETActor() -- begin\n");
  mIsRestrictedFlag = false;
  pMessenger = new GateLETActorMessenger(this);
  GateDebugMessageDec("Actor",4,"GateLETActor() -- end\n");
  emcalc = new G4EmCalculator;
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
/// Destructor
GateLETActor::~GateLETActor()  {
  delete pMessenger;
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
/// Construct
void GateLETActor::Construct() {
  GateDebugMessageInc("Actor", 4, "GateLETActor -- Construct - begin\n");
  GateVImageActor::Construct();

  // Enable callbacks
  EnableBeginOfRunAction(true);
  EnableBeginOfEventAction(true);
  EnablePreUserTrackingAction(true);
  EnableUserSteppingAction(true);

  // Output Filename
  mLETFilename = mSaveFilename;

  // Set origin, transform, flag
  SetOriginTransformAndFlagToImage(mLETImage);
  SetOriginTransformAndFlagToImage(mEdepImage);
  SetOriginTransformAndFlagToImage(mFinalImage);

  // Resize and allocate images
  mLETImage.SetResolutionAndHalfSize(mResolution, mHalfSize, mPosition);
  mLETImage.Allocate();
  mEdepImage.SetResolutionAndHalfSize(mResolution, mHalfSize, mPosition);
  mEdepImage.Allocate();
  mFinalImage.SetResolutionAndHalfSize(mResolution, mHalfSize, mPosition);
  mFinalImage.Allocate();

  // Warning: for the moment we force to PostStepHitType. This is ok
  // (slightly faster) if voxel sizes are the same between the
  // let-actor and the attached voxelized volume. But wring if not.
  mStepHitType = PostStepHitType; // Warning

  // Print information
  GateMessage("Actor", 1,
              "\tLET Actor      = '" << GetObjectName() << Gateendl <<
              "\tLET image      = " << mLETFilename << Gateendl <<
              "\tResolution     = " << mResolution << Gateendl <<
              "\tHalfSize       = " << mHalfSize << Gateendl <<
              "\tPosition       = " << mPosition << Gateendl);

  ResetData();
  GateMessageDec("Actor", 4, "GateLETActor -- Construct - end\n");
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
/// Save data
void GateLETActor::SaveData() {
  GateVActor::SaveData();

  // Final computation: divide the cumulated LET by the cumulated
  // edep.
  GateImage::const_iterator iter_LET = mLETImage.begin();
  GateImage::const_iterator iter_Edep = mEdepImage.begin();
  GateImage::iterator iter_Final = mFinalImage.begin();
  for(iter_LET = mLETImage.begin(); iter_LET != mLETImage.end(); iter_LET++) {
    if (*iter_Edep == 0.0) *iter_Final = 0.0; // do not divide by zero
    else *iter_Final = (*iter_LET)/(*iter_Edep);
    iter_Edep++;
    iter_Final++;
  }
  mFinalImage.Write(mLETFilename);
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
void GateLETActor::ResetData() {
  mLETImage.Fill(0.0);
  mEdepImage.Fill(0.0);
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
void GateLETActor::BeginOfRunAction(const G4Run * r) {
  GateVActor::BeginOfRunAction(r);
  GateDebugMessage("Actor", 3, "GateLETActor -- Begin of Run\n");
  // ResetData(); // Do no reset here !! (when multiple run);
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Callback at each event
void GateLETActor::BeginOfEventAction(const G4Event * e) {
  GateVActor::BeginOfEventAction(e);
  GateDebugMessage("Actor", 3, "GateLETActor -- Begin of Event: "<<mCurrentEvent << Gateendl);
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
void GateLETActor::UserSteppingActionInVoxel(const int index, const G4Step* step) {
  GateDebugMessageInc("Actor", 4, "GateLETActor -- UserSteppingActionInVoxel - begin\n");
  GateDebugMessageInc("Actor", 4, "enedepo = " << step->GetTotalEnergyDeposit() << Gateendl);
  GateDebugMessageInc("Actor", 4, "weight = " <<  step->GetTrack()->GetWeight() << Gateendl);

  // Get edep and current particle weight
  const double weight = step->GetTrack()->GetWeight();
  const double edep = step->GetTotalEnergyDeposit()*weight;

  // if no energy is deposited or energy is deposited outside image => do nothing
  if (edep == 0) {
    GateDebugMessage("Actor", 5, "GateLETActor edep == 0 : do nothing\n");
    return;
  }
  if (index < 0) {
    GateDebugMessage("Actor", 5, "GateLETActor pixel index < 0 : do nothing\n");
    return;
  }

  // Get somes values
  double density = step->GetPreStepPoint()->GetMaterial()->GetDensity();
  G4String material = step->GetPreStepPoint()->GetMaterial()->GetName();
  double energy1 = step->GetPreStepPoint()->GetKineticEnergy();
  double energy2 = step->GetPostStepPoint()->GetKineticEnergy();
  double energy=(energy1+energy2)/2;
  G4String partname = step->GetTrack()->GetDefinition()->GetParticleName();

  // The following variable should be used: mIsRestrictedFlag mDeltaRestricted

  // Compute the dedx for the current particle in the current material
  double dedx = emcalc->ComputeElectronicDEDX(energy, partname, material);
  double doseAveragedLET=edep*(dedx/(density*1.6e-19));

  // Store the LET
  mLETImage.AddValue(index, doseAveragedLET);

  // Store the Edep (needed for final computation)
  mEdepImage.AddValue(index, edep);

  GateDebugMessageDec("Actor", 4, "GateLETActor -- UserSteppingActionInVoxel -- end\n");
}
//-----------------------------------------------------------------------------
