# Research and references

**Evidence cut-off:** 9 November 2026. Research was checked against publisher, PubMed, FAO AGRIS or institutional records where accessible. Metadata below is limited to references whose title, author information and DOI could be corroborated. This is a starter bibliography, not a formal systematic review.

## Priority saffron literature

### Temperature and floral induction

**Wang, Z., Li, X., Xu, J., Yang, Z., & Zhang, Y. (2021).** “Effects of ambient temperature on flower initiation and flowering in saffron (*Crocus sativus* L.).” *Scientia Horticulturae*, 279, 109859. DOI: [10.1016/j.scienta.2020.109859](https://doi.org/10.1016/j.scienta.2020.109859). The study investigated temperature histories and incubation periods under the Chinese “two-segment” production model. Its summary reports a staged warm/intermediate/cool sequence associated with flower initiation and flowering. The work is especially useful for rejecting the idea that one constant chamber target is sufficient. Its numbers belong to that cultivation model; they are not validated setpoints for this prototype's aeroponic root environment.

**Jose-Santhi, J., Sheikh, F. R., Kalia, D., & Singh, R. K. (2023).** “Sugar metabolism mediates temperature-dependent flowering induction in saffron (*Crocus sativus* L.).” *Environmental and Experimental Botany*, 206, 105150. DOI: [10.1016/j.envexpbot.2022.105150](https://doi.org/10.1016/j.envexpbot.2022.105150). The paper investigates how environmental temperature relates to sugar metabolism and flowering induction in dormant corms. It provides physiological support for phase-aware treatment, but does not establish a complete control schedule for a small Peltier-cooled aeroponic chamber.

### Controlled environment, corms and agronomic context

**Askari-Khorasgani, O., & Pessarakli, M. (2019).** “Shifting saffron (*Crocus sativus* L.) culture from traditional farmland to controlled environment (greenhouse) condition to avoid the negative impact of climate changes and increase its productivity.” *Journal of Plant Nutrition*, 42(19), 2642–2665. DOI: [10.1080/01904167.2019.1659348](https://doi.org/10.1080/01904167.2019.1659348). This review discusses controlled-environment saffron cultivation and the dependence of production on temperature, corm size and agronomic management. It supports investigating protected cultivation, not a claim that indoor production automatically outperforms field production.

**Cardone, L., Castronuovo, D., Perniola, M., Cicco, N., & Candido, V. (2019).** “Evaluation of corm origin and climatic conditions on saffron (*Crocus sativus* L.) yield and quality.” *Journal of the Science of Food and Agriculture*, 99(13), 5858–5869. DOI: [10.1002/jsfa.9860](https://doi.org/10.1002/jsfa.9860). This is relevant to batch tracking: corm provenance and environment can vary, so outcomes from an uncharacterised batch should not be attributed to the controller alone.

**Pirasteh-Anosheh, H., Babaie-Zarch, M. J., Nasrabadi, M., Parnian, A., Alavi-Siney, S. M., Beyrami, H., Kaveh, H., Hashemi, S. E., Durrer, U., McDonald, K., & Race, M. (2023).** “Climate and management factors influence saffron yield in different environments.” *Agrosystems, Geosciences & Environment*, 6(3), e20418. DOI: [10.1002/agg2.20418](https://doi.org/10.1002/agg2.20418). The publisher DOI record and university repository metadata were checked. The study compared 13 fields and reports that yield is associated with multiple interacting variables, including temperature and soil/water management. Its findings are a warning against attributing crop responses to temperature alone.

**Qaryouti, M., Al-Soqeer, A., Abdelaziz, M. E., Gruda, N. S., AlSahly, S., Alrasheed, W., Althobiti, S., Babiker, O., Sharafeldin, M., & Voogt, W. (2025).** “Cutting-edge greenhouse practices for better stigma yield and corm quality of saffron (*Crocus sativus* L.).” *Frontiers in Plant Science*, 16, 1612791. DOI: [10.3389/fpls.2025.1612791](https://doi.org/10.3389/fpls.2025.1612791). The controlled-greenhouse study compared 67, 100 and 200 corms/m² and 8 cm versus 13 cm planting depth. The reported density/depth trade-offs are relevant to experimental design, but the physical planting-depth results do not directly transfer to roots suspended in an aeroponic enclosure.

**Acharya, B. S., Ghalehgolabbehbahani, A., Hamido, S., Zinati, G., Bozzolo, A., Archer, L., Wendelberger, K., Das, S., Thapa, R., & Panday, D. (2025).** “Saffron (*Crocus sativus* L.): The golden spice—management, challenges, and opportunities for sustainable production in the United States.” *Journal of Agriculture and Food Research*, 21, 101970. DOI: [10.1016/j.jafr.2025.101970](https://doi.org/10.1016/j.jafr.2025.101970). This review covers biology, agronomy, challenges, and controlled/alternative production context. Its region-specific framing should not be treated as a substitute for Kashmir/India-specific agronomic advice.

## Embedded software reference

**Espressif Systems.** “Preferences — Arduino ESP32.” [Official documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html). The `Preferences` library uses ESP32 NVS and retains values across restarts and power-loss events. It suits small configuration, not high-frequency telemetry. SAFFRON-AERO therefore stores compact configuration in dual slots and writes event records to LittleFS instead of rewriting flash with every sensor sample.

**Pallets Projects.** “Flask installation.” [Official documentation](https://flask.palletsprojects.com/en/stable/installation/). The dashboard is a local optional client. Its versions are pinned in `dashboard/requirements.txt`; confirm the dependency versions against the package index when updating them.

## Claims this project does not make

- No peer-reviewed saffron aeroponics target profile has been identified in this starter bibliography that validates the exact chamber size, Peltier hardware and root-mist layout described here.
- No expected commercial yield is calculated from the number of corms in the prototype.
- No pH, EC, nutrient concentration, root-zone moisture, PAR or UV exposure is reported unless an appropriately selected and calibrated instrument is later installed.
- No field-scale climate-resilience or weather-advisory claim follows solely from controlling a small enclosure.
- A DOI link establishes a reference location; it does not mean a full-text paper was freely available or that every statement in it was independently reproduced.

## Research portals for subsequent review

- [ICAR](https://icar.org.in/)
- [FAO AGRIS](https://agris.fao.org/)
- [Google Scholar](https://scholar.google.com/)
- [Crossref Search](https://search.crossref.org/)
- [PubMed](https://pubmed.ncbi.nlm.nih.gov/)
- [Frontiers in Plant Science: 2025 greenhouse study](https://www.frontiersin.org/journals/plant-science/articles/10.3389/fpls.2025.1612791/full)
