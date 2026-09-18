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
// ============================================================
// configureRAW
// ------------------------------------------------------------
// Monta a configuração RAW (Restricted Access Window) do
// 802.11ah (Wi-Fi HaLow) a partir do número de STAs da simulação.
//
// Divide os Nsta STAs em grupos de até 16 AIDs, cria um
// RawAssignment para cada grupo e acumula todos em um único
// RPS, que é anexado à rpslist (usada depois nos beacons).
//
// Parâmetros extraídos dos arquivos oficiais do autor
// (OptimalRawGroup/RawConfig-*.txt):
//   - SlotFormat = 1  → RAW bidirecional (exige slotNum < 8)
//   - slotNum    = 2  → valor padrão do autor
//   - slotDur    = 102 → do RawConfig-32-2-2-51200-1-0.txt
//   - 16 STAs/grupo    → evita truncamento do beacon quando
//                        há muitos assignments
// ============================================================
RPSVector configureRAW (RPSVector rpslist, std::string RAWConfigFile, Configuration &config) {

    // Container que agrupará todos os RawAssignments.
    // Esse é o objeto que efetivamente vai no beacon (via rpslist).
    RPS *m_rps = new RPS;

    // ------------------------------------------------------------
    // Constantes de configuração (valores validados pelo autor)
    // ------------------------------------------------------------
    const uint32_t SLOT_FORMAT       = 1;    // 1 = RAW bidirecional (UL+DL)
    const uint32_t SLOT_NUM          = 2;    // nº de slots por RAW
    const uint32_t SLOT_DUR          = 102;  // duração do slot (unidades de 500 µs)
    const uint32_t STAsPorAssignment = 16;   // tamanho máximo de cada grupo

    // ------------------------------------------------------------
    // Variáveis de controle do fatiamento de AIDs
    // ------------------------------------------------------------
    uint32_t N        = config.Nsta;  // total de STAs na rede
    uint32_t aidAtual = 1;            // primeiro AID do próximo grupo (AIDs começam em 1)

    // ------------------------------------------------------------
    // Laço: cria um RawAssignment por grupo de até 16 STAs
    // Ex.: N=40 → [1..16], [17..32], [33..40]
    // ------------------------------------------------------------
    while (aidAtual <= N) {

        // Último AID (inclusive) deste grupo.
        // O std::min evita ultrapassar N no último grupo (que
        // pode ter menos de 16 STAs).
        uint32_t aidFim = std::min<uint32_t>(aidAtual + STAsPorAssignment - 1, N);

        // Cria o assignment deste grupo
        RPS::RawAssignment *m_raw = new RPS::RawAssignment;

        // Preenche o subcampo RAW Slot Definition do Info Field:
        m_raw->SetRawControl (0);          // sem flags especiais (AID-only mode)
        m_raw->SetSlotCrossBoundary (1);   // slot pode atravessar a fronteira do beacon
        m_raw->SetSlotFormat (SLOT_FORMAT);// 1 → RAW bidirecional
        m_raw->SetSlotDurationCount (SLOT_DUR);
        m_raw->SetSlotNum (SLOT_NUM);

        // ------------------------------------------------------------
        // Codifica o intervalo de AIDs no campo RAW Group (24 bits):
        //   | AID End (11b) | AID Start (11b) | Page (2b) |
        //   |  bits 23..13  |   bits 12..2    |  bits 1..0|
        // ------------------------------------------------------------
        uint32_t page    = 0;                                        // mesma página de associação
        uint32_t rawinfo = (aidFim << 13) | (aidAtual << 2) | page;  // empacota os 3 campos
        m_raw->SetRawGroup (rawinfo);                                // "STAs com AID em [aidAtual..aidFim]"

        // Adiciona o assignment ao RPS (cópia) e libera o temporário
        m_rps->SetRawAssignment (*m_raw);
        delete m_raw;

        // Avança para o próximo bloco de AIDs
        aidAtual = aidFim + 1;
    }

    // ------------------------------------------------------------
    // Finalização: anexa o RPS pronto à lista e registra na config
    // ------------------------------------------------------------
    rpslist.rpsset.push_back (m_rps);  // o RPS agora faz parte do beacon
    config.NRawSta = N;                // todos os STAs ficaram cobertos por RAW
    std::cerr << "[RAW] N=" << N
              << " → " << ((N + STAsPorAssignment - 1) / STAsPorAssignment)
              << " grupos de " << STAsPorAssignment
              << ", slotNum=" << SLOT_NUM
              << ", slotDur=" << SLOT_DUR << std::endl;

    return rpslist;
}

int main (int argc, char *argv[])
{
    // Carrega as variáveis básicas
    Configuration config(argc, argv);
    
    // Lê as regras de janela estrita e joga na memória da config
    config.rps = configureRAW(config.rps, config.RAWConfigFile, config);
    
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
