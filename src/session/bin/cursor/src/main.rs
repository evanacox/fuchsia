// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use {
    anyhow::Error,
    carnelian::{
        app::Config,
        color::Color,
        make_app_assistant,
        render::{rive::load_rive, Context as RenderContext},
        scene::{
            facets::{FacetId, RiveFacet, SetSizeMessage},
            scene::{Scene, SceneBuilder},
        },
        App, AppAssistant, Size, ViewAssistant, ViewAssistantContext, ViewAssistantPtr, ViewKey,
    },
    fuchsia_trace_provider,
    fuchsia_zircon::{Event, Time},
    rive_rs::{self as rive},
    std::path::PathBuf,
};

const POINTER: &'static str = "/pkg/data/pointer.riv";
const BACKGROUND_COLOR: Color = Color { r: 0, g: 0, b: 0, a: 0 };

#[derive(Default)]
struct CursorManagerAppAssistant;

impl AppAssistant for CursorManagerAppAssistant {
    fn setup(&mut self) -> Result<(), Error> {
        Ok(())
    }

    fn create_view_assistant(&mut self, _view_key: ViewKey) -> Result<ViewAssistantPtr, Error> {
        Ok(Box::new(CursorManagerViewAssistant::new()))
    }

    fn filter_config(&mut self, config: &mut Config) {
        config.view_mode = carnelian::app::ViewMode::Hosted;
        config.input = false;
        config.needs_blending = true;
    }
}

struct SceneDetails {
    scene: Scene,
    _file: rive::File,
    rive: FacetId,
    artboard: rive::Object<rive::Artboard>,
    animation: Option<rive::animation::LinearAnimationInstance>,
}

struct CursorManagerViewAssistant {
    last_presentation_time: Option<Time>,
    scene_details: Option<SceneDetails>,
}

impl CursorManagerViewAssistant {
    fn new() -> CursorManagerViewAssistant {
        CursorManagerViewAssistant { last_presentation_time: None, scene_details: None }
    }

    fn set_size(&mut self, size: &Size) {
        if let Some(scene_details) = self.scene_details.as_mut() {
            scene_details
                .scene
                .send_message(&scene_details.rive, Box::new(SetSizeMessage { size: *size }));
        }
    }
}

impl ViewAssistant for CursorManagerViewAssistant {
    fn resize(&mut self, new_size: &Size) -> Result<(), Error> {
        self.set_size(new_size);
        Ok(())
    }

    fn render(
        &mut self,
        render_context: &mut RenderContext,
        ready_event: Event,
        context: &ViewAssistantContext,
    ) -> Result<(), Error> {
        let mut scene_details = self.scene_details.take().unwrap_or_else(|| {
            let file = load_rive(PathBuf::from(POINTER)).expect("failed to load animation");
            let mut builder = SceneBuilder::new().background_color(BACKGROUND_COLOR).mutable(false);
            let artboard = file.artboard().expect("failed to get artboard");
            let artboard_ref = artboard.as_ref();
            let rive_facet = RiveFacet::new(context.size, artboard.clone());
            let rive = builder.facet(Box::new(rive_facet));
            let scene = builder.build();
            let animation = artboard_ref
                .animations()
                .next()
                .map(|animation| rive::animation::LinearAnimationInstance::new(animation));

            SceneDetails { scene, _file: file, rive, artboard, animation }
        });

        let artboard_ref = scene_details.artboard.as_ref();
        let request_render = if let Some(animation) = scene_details.animation.as_mut() {
            let presentation_time = context.presentation_time;
            let elapsed = if let Some(last_presentation_time) = self.last_presentation_time {
                const NANOS_PER_SECOND: f32 = 1_000_000_000.0;
                (presentation_time - last_presentation_time).into_nanos() as f32 / NANOS_PER_SECOND
            } else {
                0.0
            };
            self.last_presentation_time = Some(presentation_time);

            animation.advance(elapsed);
            animation.apply(scene_details.artboard.clone(), 1.0);

            artboard_ref.advance(elapsed);
            true
        } else {
            artboard_ref.advance(0.0);
            false
        };

        scene_details.scene.render(render_context, ready_event, context)?;

        if request_render {
            context.request_render();
        }

        self.scene_details = Some(scene_details);

        Ok(())
    }
}

fn main() -> Result<(), Error> {
    fuchsia_trace_provider::trace_provider_create_with_fdio();
    App::run(make_app_assistant::<CursorManagerAppAssistant>())
}
