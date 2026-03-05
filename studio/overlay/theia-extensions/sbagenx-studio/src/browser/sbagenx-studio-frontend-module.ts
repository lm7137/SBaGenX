/********************************************************************************
 * Copyright (C) 2026 SBaGenX contributors.
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License, which is available in the project root.
 *
 * SPDX-License-Identifier: MIT
 ********************************************************************************/

import '../../src/browser/style/index.css';
import './sbagenx-studio-language';

import { bindViewContribution, FrontendApplicationContribution, RemoteConnectionProvider, ServiceConnectionProvider, WidgetFactory } from '@theia/core/lib/browser';
import { ContainerModule } from '@theia/core/shared/inversify';
import { SbagenxStudioEngineService, SBAGENX_STUDIO_ENGINE_PATH } from '../common/sbagenx-studio-engine-protocol';
import { SbagenxStudioContribution } from './sbagenx-studio-contribution';
import { SbagenxStudioModel } from './sbagenx-studio-model';
import { SbagenxStudioWidget } from './sbagenx-studio-widget';

export default new ContainerModule(bind => {
    bind(SbagenxStudioEngineService).toDynamicValue(ctx => {
        const provider = ctx.container.get<ServiceConnectionProvider>(RemoteConnectionProvider);
        return provider.createProxy<SbagenxStudioEngineService>(SBAGENX_STUDIO_ENGINE_PATH);
    }).inSingletonScope();

    bind(SbagenxStudioModel).toSelf().inSingletonScope();

    bind(SbagenxStudioWidget).toSelf();
    bind(WidgetFactory).toDynamicValue(({ container }) => ({
        id: SbagenxStudioWidget.ID,
        createWidget: () => container.get(SbagenxStudioWidget)
    })).inSingletonScope();

    bindViewContribution(bind, SbagenxStudioContribution);
    bind(FrontendApplicationContribution).toService(SbagenxStudioContribution);
});
