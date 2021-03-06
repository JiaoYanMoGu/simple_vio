//
// Created by lancelot on 4/18/17.
//

#include "util/setting.h"
#include "BundleAdjustemt.h"
#include "./Implement/SimpleBA.h"

BundleAdjustemt::BundleAdjustemt(int type) {
	switch (type) {
		case SIMPLE_BA :
			impl = std::make_shared<SimpleBA>();
			break;
	}
}

bool BundleAdjustemt::run(std::vector<std::shared_ptr<viFrame>> &viframes,
                          BundleAdjustemt::obsModeType &obsModes,
                          std::vector<std::shared_ptr<imuFactor>> &imufactors, int iter_) {
	return impl->run(viframes, obsModes, imufactors, iter_);
}