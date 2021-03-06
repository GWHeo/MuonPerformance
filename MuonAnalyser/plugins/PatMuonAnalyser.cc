#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/ESHandle.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"
#include "FWCore/Utilities/interface/InputTag.h"

#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"

#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/PatCandidates/interface/Muon.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "SimDataFormats/PileupSummaryInfo/interface/PileupSummaryInfo.h"

#include "Geometry/Records/interface/MuonGeometryRecord.h"
#include "Geometry/GEMGeometry/interface/ME0Geometry.h"

#include <vector>
#include <TTree.h>
#include <TFile.h>
#include <TLorentzVector.h>

using namespace std;

class PatMuonAnalyser : public edm::one::EDAnalyzer<edm::one::SharedResources>  {

public:
  explicit PatMuonAnalyser(const edm::ParameterSet&);
  ~PatMuonAnalyser(){};

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  virtual void analyze(const edm::Event&, const edm::EventSetup&) override;

  void setBranches(TTree *tree);
  void fillBranches(TTree *tree, TLorentzVector &tlv, const pat::Muon *muon, bool isSignal, int pdgId);

  bool isSignalMuon(const reco::GenParticle &gen);
  
  bool isME0MuonSelNew(reco::Muon muon, double dEtaCut, double dPhiCut, double dPhiBendCut);
  
  edm::EDGetTokenT<std::vector<reco::Vertex>> verticesToken_;
  edm::EDGetTokenT<std::vector<pat::Muon>> muonsToken_;
  edm::EDGetTokenT<edm::View<reco::GenParticle> > prunedGenToken_;
  edm::EDGetTokenT<std::vector<PileupSummaryInfo>> putoken_;

  reco::Vertex priVertex_;
  
  const ME0Geometry* ME0Geometry_;
  
  TTree* genttree_;
  TTree* recottree_;
  TH1D* h_nevents;

  int b_pu_density, b_pu_numInteractions;
  int b_nvertex;
  
  TLorentzVector b_muon;
  bool b_muon_signal;
  int b_muon_pdgId;
  int b_muon_no;
  float b_muon_poszPV0, b_muon_poszMuon;
  bool b_muon_isTight, b_muon_isMedium, b_muon_isLoose;
  bool b_muon_isME0MuonTight, b_muon_isME0MuonMedium, b_muon_isME0MuonLoose;

  float b_muon_PFIso04; float b_muon_PFIso03;
  float b_muon_PFIso03ChargedHadronPt, b_muon_PFIso03NeutralHadronEt;
  float b_muon_PFIso03PhotonEt, b_muon_PFIso03PUPt;
  float b_muon_PFIso04ChargedHadronPt, b_muon_PFIso04NeutralHadronEt;
  float b_muon_PFIso04PhotonEt, b_muon_PFIso04PUPt;
  float b_muon_TrkIso05; float b_muon_TrkIso03;
  float b_muon_puppiIso, b_muon_puppiIsoNoLep;
  float b_muon_puppiIso_ChargedHadron, b_muon_puppiIso_NeutralHadron, b_muon_puppiIso_Photon;  
  float b_muon_puppiIsoNoLep_ChargedHadron, b_muon_puppiIsoNoLep_NeutralHadron, b_muon_puppiIsoNoLep_Photon;  

};
PatMuonAnalyser::PatMuonAnalyser(const edm::ParameterSet& iConfig):
  verticesToken_(consumes<std::vector<reco::Vertex>>(iConfig.getParameter<edm::InputTag>("vertices"))),
  muonsToken_(consumes<std::vector<pat::Muon>>(iConfig.getParameter<edm::InputTag>("muons"))),
  prunedGenToken_(consumes<edm::View<reco::GenParticle> >(iConfig.getParameter<edm::InputTag>("pruned")))
{
  putoken_ = consumes<std::vector<PileupSummaryInfo>>(edm::InputTag("addPileupInfo"));
  
  usesResource("TFileService");
  edm::Service<TFileService> fs;
  h_nevents = fs->make<TH1D>("nevents", "nevents", 1, 0, 1);
  
  genttree_ = fs->make<TTree>("gen", "gen");
  setBranches(genttree_);
  recottree_ = fs->make<TTree>("reco", "reco");
  setBranches(recottree_);
}

void PatMuonAnalyser::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup)
{
  h_nevents->Fill(0.5);
  
  edm::ESHandle<ME0Geometry> hGeom;
  iSetup.get<MuonGeometryRecord>().get(hGeom);
  ME0Geometry_ =( &*hGeom);
  
  using namespace edm;
  Handle<std::vector<reco::Vertex>> vertices;
  iEvent.getByToken(verticesToken_, vertices);
  // Vertices
  int prVtx = -1;
  for (size_t i = 0; i < vertices->size(); i++) {
    if (vertices->at(i).isFake()) continue;
    if (vertices->at(i).ndof() <= 4) continue;
    if (prVtx < 0) prVtx = i;
  }
  if (prVtx < 0) return;
  priVertex_ = vertices->at(prVtx);
  b_nvertex = vertices->size();

  Handle<std::vector<pat::Muon>> muons;
  iEvent.getByToken(muonsToken_, muons);

  Handle<edm::View<reco::GenParticle> > pruned;
  iEvent.getByToken(prunedGenToken_,pruned);

  // edm::Handle<std::vector <PileupSummaryInfo> > PupInfo;
  // iEvent.getByToken(putoken, PupInfo);
  // b_pu_density = 0; b_pu_numInteractions = 0;
  // std::vector<PileupSummaryInfo>::const_iterator ipu;
  // for (ipu = PupInfo->begin(); ipu != PupInfo->end(); ++ipu) {
  //   if ( ipu->getBunchCrossing() != 0 ) continue; // storing detailed PU info only for BX=0
  //   for (unsigned int i=0; i<ipu->getPU_zpositions().size(); ++i) {
  //     if ( abs((ipu->getPU_zpositions())[i] - simVertex_.position().z()) < 0.1 )
  // 	++b_pu_density;
  //     ++b_pu_numInteractions;
  //   }
  // }

  b_muon_no = 0;
  for (const reco::GenParticle &gen : *pruned) {
    if (!isSignalMuon(gen)) continue;
    TLorentzVector gentlv(gen.momentum().x(), gen.momentum().y(), gen.momentum().z(), gen.energy() );

    const pat::Muon *recoMu = NULL;
    for (const pat::Muon &muon : *muons) {
      TLorentzVector recotlv(muon.momentum().x(), muon.momentum().y(), muon.momentum().z(), muon.energy() );
      if (gentlv.DeltaR(recotlv) < 0.1)
	recoMu = &muon;
    }
    //if ( recoMu == NULL ) continue;
    fillBranches(genttree_, gentlv, recoMu, true, gen.pdgId());
  }

  
  b_muon_no = 0;
  
  for (const pat::Muon &muon : *muons) {
    if (muon.pt() < 2.) continue;
    if (abs(muon.eta()) > 2.8) continue;

    bool isSignal = false;
    const reco::GenParticle *gen = muon.genLepton();
    int pdgId = 0;
    if (gen){
      pdgId = gen->pdgId();
      isSignal = isSignalMuon(*gen);
    }
  
    TLorentzVector recotlv(muon.momentum().x(), muon.momentum().y(), muon.momentum().z(), muon.energy() );
    fillBranches(recottree_, recotlv, &muon, isSignal, pdgId);
  }
  
  return;
}

bool PatMuonAnalyser::isSignalMuon(const reco::GenParticle &gen)
{
  if (abs(gen.pdgId()) != 13) return false;
  
  for (unsigned int i = 0; i < gen.numberOfMothers(); ++i){
    //In case of pdgId() = 23, indicate Z-boson. if it's 25, that becomes higgs.
    if (gen.mother(i)->pdgId() == 23 || gen.mother(i)->pdgId() == 25){
      return true;
    }
  }
  return false;
  
}

void PatMuonAnalyser::fillBranches(TTree *tree, TLorentzVector &tlv, const pat::Muon *muon, bool isSignal, int pdgId)
{
  b_muon = tlv;
  b_muon_signal = isSignal;
  b_muon_pdgId = pdgId;
  ++b_muon_no;
  
  b_muon_poszPV0  = 0;
  b_muon_poszMuon = 0;
    
  b_muon_isTight = 0; b_muon_isMedium = 0; b_muon_isLoose = 0;
  
  b_muon_PFIso04 = 0;  b_muon_PFIso03 = 0;
  b_muon_PFIso03ChargedHadronPt = 0; b_muon_PFIso03NeutralHadronEt = 0;
  b_muon_PFIso03PhotonEt = 0; b_muon_PFIso03PUPt = 0;
  b_muon_PFIso04ChargedHadronPt = 0; b_muon_PFIso04NeutralHadronEt = 0;
  b_muon_PFIso04PhotonEt = 0; b_muon_PFIso04PUPt = 0;
  b_muon_TrkIso05 = 0;  b_muon_TrkIso03 = 0;
  b_muon_puppiIso = 0; b_muon_puppiIso_ChargedHadron = 0; b_muon_puppiIso_NeutralHadron = 0; b_muon_puppiIso_Photon = 0;
  b_muon_puppiIsoNoLep = 0; b_muon_puppiIsoNoLep_ChargedHadron = 0; b_muon_puppiIsoNoLep_NeutralHadron = 0; b_muon_puppiIsoNoLep_Photon = 0;  

  b_muon_isME0MuonTight = 0; b_muon_isME0MuonMedium = 0; b_muon_isME0MuonLoose = 0;

  if (muon){
    b_muon_poszPV0  = priVertex_.position().z();
    b_muon_poszMuon = muon->muonBestTrack()->vz();
    
    b_muon_TrkIso03 = muon->isolationR03().sumPt/muon->pt();
    b_muon_TrkIso05 = muon->isolationR05().sumPt/muon->pt();
    
    b_muon_PFIso03ChargedHadronPt = muon->pfIsolationR03().sumChargedHadronPt;
    b_muon_PFIso03NeutralHadronEt = muon->pfIsolationR03().sumNeutralHadronEt;
    b_muon_PFIso03PhotonEt        = muon->pfIsolationR03().sumPhotonEt;
    b_muon_PFIso03PUPt            = muon->pfIsolationR03().sumPUPt;

    b_muon_PFIso04ChargedHadronPt = muon->pfIsolationR04().sumChargedHadronPt;
    b_muon_PFIso04NeutralHadronEt = muon->pfIsolationR04().sumNeutralHadronEt;
    b_muon_PFIso04PhotonEt        = muon->pfIsolationR04().sumPhotonEt;
    b_muon_PFIso04PUPt            = muon->pfIsolationR04().sumPUPt;   
    
    b_muon_PFIso04 = (muon->pfIsolationR04().sumChargedHadronPt + TMath::Max(0.,muon->pfIsolationR04().sumNeutralHadronEt + muon->pfIsolationR04().sumPhotonEt - 0.5*muon->pfIsolationR04().sumPUPt))/muon->pt();
    b_muon_PFIso03 = (muon->pfIsolationR03().sumChargedHadronPt + TMath::Max(0.,muon->pfIsolationR03().sumNeutralHadronEt + muon->pfIsolationR03().sumPhotonEt - 0.5*muon->pfIsolationR03().sumPUPt))/muon->pt();
    
    b_muon_puppiIso_ChargedHadron = muon->puppiChargedHadronIso();
    b_muon_puppiIso_NeutralHadron = muon->puppiNeutralHadronIso();
    b_muon_puppiIso_Photon = muon->puppiPhotonIso();
    b_muon_puppiIso = (b_muon_puppiIso_ChargedHadron+b_muon_puppiIso_NeutralHadron+b_muon_puppiIso_Photon)/muon->pt();
    b_muon_puppiIsoNoLep_ChargedHadron = muon->puppiNoLeptonsChargedHadronIso();
    b_muon_puppiIsoNoLep_NeutralHadron = muon->puppiNoLeptonsNeutralHadronIso();
    b_muon_puppiIsoNoLep_Photon = muon->puppiNoLeptonsPhotonIso();
    b_muon_puppiIsoNoLep = (b_muon_puppiIsoNoLep_ChargedHadron+b_muon_puppiIsoNoLep_NeutralHadron+b_muon_puppiIsoNoLep_Photon)/muon->pt(); 
    b_muon_isTight = muon::isTightMuon(*muon, priVertex_);
    b_muon_isMedium = muon::isMediumMuon(*muon);
    b_muon_isLoose = muon::isLooseMuon(*muon);
    
    double mom = muon->p();
    double dPhiCut_ = std::min(std::max(1.2/mom,1.2/100),0.056);
    double dPhiBendCut_ = std::min(std::max(0.2/mom,0.2/100),0.0096);
    b_muon_isME0MuonLoose = isME0MuonSelNew(*muon, 0.077, dPhiCut_, dPhiBendCut_);
    
    bool ipxy = false, ipz = false, validPxlHit = false, highPurity = false;
    if (muon->innerTrack().isNonnull()){
      ipxy = std::abs(muon->muonBestTrack()->dxy(priVertex_.position())) < 0.2;
      ipz = std::abs(muon->muonBestTrack()->dz((priVertex_.position()))) < 0.5;
      validPxlHit = muon->innerTrack()->hitPattern().numberOfValidPixelHits() > 0;
      highPurity = muon->innerTrack()->quality(reco::Track::highPurity);
    }
    // isMediumME0 - just loose with track requirements for now, this needs to be updated
    b_muon_isME0MuonMedium = isME0MuonSelNew(*muon, 0.077, dPhiCut_, dPhiBendCut_) && ipxy && validPxlHit && highPurity;

    // tighter cuts for tight ME0
    dPhiCut_ = std::min(std::max(1.2/mom,1.2/100),0.032);
    dPhiBendCut_ = std::min(std::max(0.2/mom,0.2/100),0.0041);
    b_muon_isME0MuonTight = isME0MuonSelNew(*muon, 0.048, dPhiCut_, dPhiBendCut_) && ipxy && ipz && validPxlHit && highPurity;
  }
  tree->Fill();
}

bool PatMuonAnalyser::isME0MuonSelNew(reco::Muon muon, double dEtaCut, double dPhiCut, double dPhiBendCut)
{
  bool result = false;
  bool isME0 = muon.isME0Muon();
    
  if(isME0){
      
    double deltaEta = 999;
    double deltaPhi = 999;
    double deltaPhiBend = 999;

    const std::vector<reco::MuonChamberMatch>& chambers = muon.matches();
    for( std::vector<reco::MuonChamberMatch>::const_iterator chamber = chambers.begin(); chamber != chambers.end(); ++chamber ){
        
      if (chamber->detector() == 5){
          
	for ( std::vector<reco::MuonSegmentMatch>::const_iterator segment = chamber->me0Matches.begin(); segment != chamber->me0Matches.end(); ++segment ){

	  LocalPoint trk_loc_coord(chamber->x, chamber->y, 0);
	  LocalPoint seg_loc_coord(segment->x, segment->y, 0);
	  LocalVector trk_loc_vec(chamber->dXdZ, chamber->dYdZ, 1);
	  LocalVector seg_loc_vec(segment->dXdZ, segment->dYdZ, 1);
            
	  const ME0Chamber * me0chamber = ME0Geometry_->chamber(chamber->id);
            
	  GlobalPoint trk_glb_coord = me0chamber->toGlobal(trk_loc_coord);
	  GlobalPoint seg_glb_coord = me0chamber->toGlobal(seg_loc_coord);
            
	  //double segDPhi = segment->me0SegmentRef->deltaPhi();
	  // need to check if this works
	  double segDPhi = me0chamber->computeDeltaPhi(seg_loc_coord, seg_loc_vec);
	  double trackDPhi = me0chamber->computeDeltaPhi(trk_loc_coord, trk_loc_vec);
            
	  deltaEta = std::abs(trk_glb_coord.eta() - seg_glb_coord.eta() );
	  deltaPhi = std::abs(trk_glb_coord.phi() - seg_glb_coord.phi() );
	  deltaPhiBend = std::abs(segDPhi - trackDPhi);
            
	  if (deltaEta < dEtaCut && deltaPhi < dPhiCut && deltaPhiBend < dPhiBendCut) result = true;
            
	}
      }
    }
      
  }
    
  return result;
    
}

void PatMuonAnalyser::setBranches(TTree *tree)
{
  tree->Branch("nvertex", &b_nvertex, "nvertex/I");
  tree->Branch("pu_density", &b_pu_density, "pu_density/I");
  tree->Branch("pu_numInteractions", &b_pu_numInteractions, "pu_numInteractions/I");
  tree->Branch("muon", "TLorentzVector", &b_muon);  
  tree->Branch("muon_no", &b_muon_no, "muon_no/I");
  tree->Branch("muon_pdgId", &b_muon_pdgId, "muon_pdgId/I");
  tree->Branch("muon_poszPV0",&b_muon_poszPV0,"muon_poszPV0/F");
  tree->Branch("muon_poszMuon",&b_muon_poszMuon,"muon_poszMuon/F");
  tree->Branch("muon_signal", &b_muon_signal, "muon_signal/O");
  tree->Branch("muon_isTight", &b_muon_isTight, "muon_isTight/O");
  tree->Branch("muon_isMedium", &b_muon_isMedium, "muon_isMedium/O");
  tree->Branch("muon_isLoose", &b_muon_isLoose, "muon_isLoose/O");
  tree->Branch("muon_isME0MuonTight", &b_muon_isME0MuonTight, "muon_isME0MuonTight/O");
  tree->Branch("muon_isME0MuonMedium", &b_muon_isME0MuonMedium, "muon_isME0MuonMedium/O");
  tree->Branch("muon_isME0MuonLoose", &b_muon_isME0MuonLoose, "muon_isME0MuonLoose/O");

  tree->Branch("muon_TrkIsolation03",&b_muon_TrkIso03,"muon_TrkIsolation03/F");
  tree->Branch("muon_TrkIsolation05",&b_muon_TrkIso05,"muon_TrkIsolation05/F");
  tree->Branch("muon_PFIsolation04",&b_muon_PFIso04,"muon_PFIsolation04/F");
  tree->Branch("muon_PFIsolation03",&b_muon_PFIso03,"muon_PFIsolation03/F");
  tree->Branch("muon_PFIso03ChargedHadronPt",&b_muon_PFIso03ChargedHadronPt,"muon_PFIso03ChargedHadronPt/F");
  tree->Branch("muon_PFIso03NeutralHadronEt",&b_muon_PFIso03NeutralHadronEt,"muon_PFIso03NeutralHadronEt/F");
  tree->Branch("muon_PFIso03PhotonEt",&b_muon_PFIso03PhotonEt,"muon_PFIso03PhotonEt/F");
  tree->Branch("muon_PFIso03PUPt",&b_muon_PFIso03PUPt,"muon_PFIso03PUPt/F");
  tree->Branch("muon_PFIso04ChargedHadronPt",&b_muon_PFIso04ChargedHadronPt,"muon_PFIso04ChargedHadronPt/F");
  tree->Branch("muon_PFIso04NeutralHadronEt",&b_muon_PFIso04NeutralHadronEt,"muon_PFIso04NeutralHadronEt/F");
  tree->Branch("muon_PFIso04PhotonEt",&b_muon_PFIso04PhotonEt,"muon_PFIso04PhotonEt/F");
  tree->Branch("muon_PFIso04PUPt",&b_muon_PFIso04PUPt,"muon_PFIso04PUPt/F");
  tree->Branch("muon_puppiIso",&b_muon_puppiIso,"muon_puppiIso/F");
  tree->Branch("muon_puppiIso_ChargedHadron",&b_muon_puppiIso_ChargedHadron,"muon_puppiIso_ChargedHadron/F");
  tree->Branch("muon_puppiIso_NeutralHadron",&b_muon_puppiIso_NeutralHadron,"muon_puppiIso_NeutralHadron/F");
  tree->Branch("muon_puppiIso_Photon",&b_muon_puppiIso_Photon,"muon_puppiIso_Photon/F");
  tree->Branch("muon_puppiIsoNoLep",&b_muon_puppiIsoNoLep,"muon_puppiIsoNoLep/F");
  tree->Branch("muon_puppiIsoNoLep_ChargedHadron",&b_muon_puppiIsoNoLep_ChargedHadron,"muon_puppiIsoNoLep_ChargedHadron/F");
  tree->Branch("muon_puppiIsoNoLep_NeutralHadron",&b_muon_puppiIsoNoLep_NeutralHadron,"muon_puppiIsoNoLep_NeutralHadron/F");
  tree->Branch("muon_puppiIsoNoLep_Photon",&b_muon_puppiIsoNoLep_Photon,"muon_puppiIsoNoLep_Photon/F");
}

void
PatMuonAnalyser::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  //The following says we do not know what parameters are allowed so do no validation
  // Please change this to state exactly what you do use, even if it is no parameters
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault(desc);
}

//define this as a plug-in
DEFINE_FWK_MODULE(PatMuonAnalyser);
