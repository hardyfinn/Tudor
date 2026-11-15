#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelObject);
	p->addModel(modelCritter);
	p->addModel(modelSeek);
	p->addModel(modelWander);
	p->addModel(modelPulser);
	p->addModel(modelShift);
	p->addModel(modelTuned);
	p->addModel(modelSideband);
	p->addModel(modelLoop);
	p->addModel(modelSwell);
	p->addModel(modelPhoneme);
	p->addModel(modelXGate);
}
