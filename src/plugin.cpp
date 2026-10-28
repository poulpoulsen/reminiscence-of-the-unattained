#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;

	p->addModel(modelMarf248);
	p->addModel(modelMarf248Compact);
	p->addModel(modelMarf248TriggerExpander);
	p->addModel(modelMarf248CvExpander);
	p->addModel(modelEuropa);
	p->addModel(modelMinerva);
	p->addModel(modelElena);
	p->addModel(modelAnnaMaria);
}
