#include "ns3/core-module.h"
#include "modulesDataset.h"
#include "../rca/Configuration.cc" 
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiHalowDatasetExample");

/**
 * @brief Lê e traduz as regras de escalonamento RAW a partir de um arquivo de texto.
 * 
 * @details Funciona como o "tradutor" das regras de negócio do projeto. Ele abre o 
 * arquivo de configuração (ex: RawConfig-rca.txt), extrai a distribuição matemática 
 * dos nós (AIDs), grupos e slots de tempo  e os converte nas estruturas C++ exigidas pela placa de rede do protocolo 802.11ah.
 * 
 * @param rpslist Estrutura base de conjuntos de parâmetros RAW (RPS) a ser preenchida.
 * @param RAWConfigFile Caminho para o arquivo de texto contendo a matriz de configuração.
 * @param config Referência ao objeto de configuração global para atualizar o número total de nós.
 * @return RPSVector O vetor estruturado com todos os grupos e regras de acesso restrito.
 */
RPSVector configureRAW(RPSVector rpslist, std::string RAWConfigFile, Configuration &config) {
    uint16_t NRPS = 0;
    uint16_t NRAWPERBEACON = 0;
    uint16_t Value = 0;
    uint32_t page = 0;
    uint32_t aid_start = 0;
    uint32_t aid_end = 0;
    uint32_t rawinfo = 0;

    std::ifstream myfile(RAWConfigFile);
    if (myfile.is_open()) {
        myfile >> NRPS;
        int totalNumSta = 0;
        for (uint16_t kk = 0; kk < NRPS; kk++) {
            RPS *m_rps = new RPS;
            myfile >> NRAWPERBEACON;
            for (uint16_t i = 0; i < NRAWPERBEACON; i++) {
                RPS::RawAssignment *m_raw = new RPS::RawAssignment;
                myfile >> Value; m_raw->SetRawControl(Value);
                myfile >> Value; m_raw->SetSlotCrossBoundary(Value);
                myfile >> Value; m_raw->SetSlotFormat(Value);
                myfile >> Value; m_raw->SetSlotDurationCount(Value);
                myfile >> Value; m_raw->SetSlotNum(Value);
                myfile >> page;
                myfile >> aid_start;
                myfile >> aid_end;
                
                rawinfo = (aid_end << 13) | (aid_start << 2) | page;
                m_raw->SetRawGroup(rawinfo);
                totalNumSta += aid_end - aid_start + 1;
                m_rps->SetRawAssignment(*m_raw);
                delete m_raw;
            }
            rpslist.rpsset.push_back(m_rps);
        }
        myfile.close();
        config.NRawSta = totalNumSta;
    } else {
        std::cout << "Unable to open RAW configuration file: " << RAWConfigFile << "\n";
    }
    return rpslist;
}

int main (int argc, char *argv[])
{
    // Carrega as variáveis básicas
    Configuration config(argc, argv);
    
    // Lê as regras de janela estrita e joga na memória da config
    config.rps = configureRAW(config.rps, config.RAWConfigFile, config);
    config.Nsta = config.NRawSta;
    
    // Estrutura as páginas (Slicing) exigidas pela placa HaLow
    config.pageS.SetPageindex (config.pageIndex);
    config.pageS.SetPagePeriod (config.pagePeriod);
    config.pageS.SetPageSliceLen (config.pageSliceLength);
    config.pageS.SetPageSliceCount (config.pageSliceCount);
    config.pageS.SetBlockOffset (config.blockOffset);
    config.pageS.SetTIMOffset (config.timOffset);
    config.tim.SetPageIndex (config.pageIndex);
    config.tim.SetDTIMPeriod (config.pageSliceCount ? config.pageSliceCount : 1);

    // parâmetros estáticos do projeto
    double distance = 2.0;
    uint32_t packetSize = 512;
    std::string outputFile = "scratch/dataset-halow.txt";

    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    HalowExperiment experiment (distance, packetSize, 200.0, config);
    
    experiment.Run ();
    experiment.ExportDataset (outputFile);

    Simulator::Destroy ();
    return 0;
}