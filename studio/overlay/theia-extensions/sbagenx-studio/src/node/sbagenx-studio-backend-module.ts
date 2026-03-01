/********************************************************************************
 * Copyright (C) 2026 SBaGenX contributors.
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License, which is available in the project root.
 *
 * SPDX-License-Identifier: MIT
 ********************************************************************************/

import { ConnectionHandler, RpcConnectionHandler } from '@theia/core';
import { ContainerModule } from '@theia/core/shared/inversify';
import { SbagenxStudioEngineService, SBAGENX_STUDIO_ENGINE_PATH } from '../common/sbagenx-studio-engine-protocol';
import { SbagenxStudioEngineServiceImpl } from './sbagenx-studio-engine-service';

export default new ContainerModule(bind => {
    bind(SbagenxStudioEngineServiceImpl).toSelf().inSingletonScope();
    bind(SbagenxStudioEngineService).toService(SbagenxStudioEngineServiceImpl);
    bind(ConnectionHandler).toDynamicValue(ctx =>
        new RpcConnectionHandler(SBAGENX_STUDIO_ENGINE_PATH, () => ctx.container.get(SbagenxStudioEngineService))
    ).inSingletonScope();
});
