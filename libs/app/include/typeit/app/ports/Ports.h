// Every port in one include, plus the clock.
//
// `core::IClock` is a port too — it is the one the domain declares for itself,
// because time is an input to the model rather than a service around it
// (TECHNICAL section 1.3). It is re-exported here so a composition root has
// one place to look.
#ifndef TYPEIT_APP_PORTS_PORTS_H
#define TYPEIT_APP_PORTS_PORTS_H

#include "typeit/app/ports/IAssetLocator.h"
#include "typeit/app/ports/IConfigStore.h"
#include "typeit/app/ports/IFileSystem.h"
#include "typeit/app/ports/IHistoryRepository.h"
#include "typeit/app/ports/ITextLibraryRepository.h"
#include "typeit/core/util/IClock.h"

namespace typeit::app {

    using core::IClock;

}  // namespace typeit::app

#endif  // TYPEIT_APP_PORTS_PORTS_H
