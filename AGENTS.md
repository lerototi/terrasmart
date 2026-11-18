# Premissas de manutenção

- Antes de implementar uma funcionalidade, pesquise a implementação, os contratos
  e os ambientes equivalentes para ESP32 e ESP8266, inclusive firmwares piloto.
- Reaproveite e mantenha em um único local a lógica e a interface comuns; isole
  apenas as diferenças necessárias de hardware, bibliotecas e persistência.
  Documente limitações que impeçam compartilhar uma funcionalidade.
- Preserve as configurações persistidas, a compatibilidade dos protocolos e o
  comportamento existente; descreva qualquer migração ou exceção.
- Para alterações compartilhadas, compile os ambientes afetados de ambas as
  plataformas e registre separadamente os testes reais de bancada.
