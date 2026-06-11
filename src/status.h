#pragma once

struct ModelStatus {
    bool haikuUp;
    bool sonnetUp;
    bool opusUp;
    bool fableUp;
    bool ok;       // true once at least one fetch has succeeded
};

bool fetchModelStatus(ModelStatus& out);
