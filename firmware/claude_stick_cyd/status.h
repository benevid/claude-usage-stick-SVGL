#pragma once

// Saúde dos modelos via status.claude.com. Incidentes não resolvidos citam a
// família do modelo no texto ("Elevated errors on Claude Opus 4.6"), então um
// scan de palavra-chave é todo o parse necessário.
struct ModelStatus {
    bool haikuUp;
    bool sonnetUp;
    bool opusUp;
    bool fableUp;
    bool ok;       // true depois que ao menos um fetch teve sucesso
};

bool fetchModelStatus(ModelStatus& out);
