// Optional post-TAA DLSS SR evaluation, adapted from todojuegos67-bot/KytyPS5
// e0b0e201651003fc839b4221c18b2022f613360c (frame-gen.cpp).
// Included inside FrameGen's private namespace with the Streamline API available.
// The game's tone mapping and HDR encoding remain in the final composite.
FrameInputs g_sr_inputs;
VulkanImage g_sr_output;
vk::ImageView g_sr_output_view = nullptr;

bool ConfigureSrImage(GraphicContext& graphics, VulkanImage& image, vk::ImageView& view,
                      vk::Format format, vk::Extent2D extent, bool output) {
	// Keep allocations through scheduler shutdown. The first comparison uses fixed
	// dimensions: do not delete images while earlier unsubmitted work still uses them.
	if (image.image != nullptr)
		return view != nullptr && image.format == format && image.extent.width == extent.width &&
		       image.extent.height == extent.height;
	vk::ImageCreateInfo create {};
	create.imageType = vk::ImageType::e2D;
	create.extent = vk::Extent3D {extent.width, extent.height, 1};
	create.mipLevels = 1;
	create.arrayLayers = 1;
	create.format = format;
	create.tiling = vk::ImageTiling::eOptimal;
	create.usage = vk::ImageUsageFlagBits::eSampled |
	               (output ? vk::ImageUsageFlagBits::eStorage : vk::ImageUsageFlagBits::eTransferDst);
	create.sharingMode = vk::SharingMode::eExclusive;
	create.samples = vk::SampleCountFlagBits::e1;
	if (!graphics.CreateImage(create, image)) return false;
	vk::ImageViewCreateInfo info {};
	info.image = image.image;
	info.viewType = vk::ImageViewType::e2D;
	info.format = format;
	info.subresourceRange = {IsDepthFormat(format) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
	                        0, 1, 0, 1};
	if (graphics.device.createImageView(&info, nullptr, &view) == vk::Result::eSuccess) return true;
	graphics.DeleteImage(image);
	return false;
}

sl::Constants MakeSrConstants(bool usable, const Camera& camera_in, vk::Extent2D render) {
	sl::Constants constants {};
	const auto    camera = usable ? Decompose(camera_in.view_proj) : CameraFrame {};
	Mat4          identity;
	for (uint32_t i = 0; i < 4; ++i) {
		identity[i] = sl::float4(i == 0, i == 1, i == 2, i == 3);
	}
	constants.cameraViewToClip = identity;
	constants.clipToCameraView = identity;
	constants.clipToLensClip   = identity;
	constants.clipToPrevClip   = identity;
	constants.prevClipToClip   = identity;
	constants.jitterOffset     = sl::float2(0.0f, 0.0f);
	constants.mvecScale        = sl::float2(1.0f, 1.0f);
	constants.cameraPinholeOffset = sl::float2(0.0f, 0.0f);
	constants.cameraPos        = sl::float3(0, 0, 0);
	constants.cameraUp         = sl::float3(0, 1, 0);
	constants.cameraRight      = sl::float3(1, 0, 0);
	constants.cameraFwd        = sl::float3(0, 0, 1);
	constants.cameraNear       = 0.1f;
	constants.cameraFar        = 10000.0f;
	constants.cameraFOV        = 1.0f;
	constants.cameraAspectRatio = 16.0f / 9.0f;
	constants.motionVectorsInvalidValue = sl::INVALID_FLOAT;
	constants.depthInverted       = sl::Boolean::eTrue;
	constants.cameraMotionIncluded = sl::Boolean::eTrue;
	constants.motionVectors3D     = sl::Boolean::eFalse;
	constants.reset               = usable ? sl::Boolean::eFalse : sl::Boolean::eTrue;
	constants.orthographicProjection = sl::Boolean::eFalse;
	constants.motionVectorsDilated   = sl::Boolean::eFalse;
	constants.motionVectorsJittered  = sl::Boolean::eFalse;
	if (usable && camera.valid) {
		// Projection in view space: x' = sx*x + cx*z, y' = sy*y + cy*z, z' = near, w' = z.
		Mat4 projection;
		projection[0] = sl::float4(camera.sx, 0, 0, 0);
		projection[1] = sl::float4(0, camera.sy, 0, 0);
		projection[2] = sl::float4(camera.cx, camera.cy, 0, 1);
		projection[3] = sl::float4(0, 0, camera.z_near, 0);
		constants.cameraViewToClip = projection;
		Inverted(projection, constants.clipToCameraView);
		const auto current  = Transposed(camera_in.view_proj);
		const auto previous = Transposed(camera_in.prev_view_proj);
		Mat4       clip_to_world;
		if (Inverted(current, clip_to_world)) {
			constants.clipToPrevClip = Multiply(clip_to_world, previous);
			Inverted(constants.clipToPrevClip, constants.prevClipToClip);
		}
		const float width  = static_cast<float>(render.width);
		const float height = static_cast<float>(render.height);
		constants.jitterOffset = sl::float2(camera_in.jitter_ndc[0] * width * 0.5f,
		                                    -camera_in.jitter_ndc[1] * height * 0.5f);
		static const bool pixels_mv = [] {
			const char* mv = std::getenv("KYTY_FRAMEGEN_MV");
			return mv != nullptr && std::string(mv) == "px";
		}();
		// The game's vectors point along the motion, in UV units: its TAA reads the history at
		// uv - mv. DLSS wants them towards the previous position (uv + mv = previous uv), so they
		// are negated; with them as they were, generated frames moved the scene the wrong way
		// when the camera turned. KYTY_FRAMEGEN_MV_SIGN=1 keeps the game's sign (a comparison).
		static const float mv_sign = [] {
			const char* sign = std::getenv("KYTY_FRAMEGEN_MV_SIGN");
			return sign != nullptr && std::atof(sign) > 0 ? 1.0f : -1.0f;
		}();
		constants.mvecScale = pixels_mv ? sl::float2(mv_sign / width, mv_sign / height) : sl::float2(mv_sign, mv_sign);
		constants.cameraPos   = camera.position;
		constants.cameraUp    = camera.up;
		constants.cameraRight = camera.right;
		constants.cameraFwd   = camera.forward;
		constants.cameraNear  = camera.z_near;
		constants.cameraFar   = 100000.0f;
		constants.cameraFOV   = 2.0f * std::atan(1.0f / std::fabs(camera.sy));
		constants.cameraAspectRatio = std::fabs(camera.sy / camera.sx);
	}
	return constants;
}

// DLSS super resolution of the composite's 3D image (UpscaleComposite): viewport 1,
// sharing the presented frame's identity with FG and serializing the SDK tag lifetime.
// The images' layouts as given (the composite path: color, depth and motion SHADER_READ_ONLY, output GENERAL; the TAA
// path: as the TAA dispatch's descriptors have them). `taa`: in place of the game's TAA (viewport 2): its color is
// jittered (the camera's jitter given), its output the TAA's.
struct DlssLayouts {
	vk::ImageLayout color  = vk::ImageLayout::eShaderReadOnlyOptimal;
	vk::ImageLayout depth  = vk::ImageLayout::eShaderReadOnlyOptimal;
	vk::ImageLayout motion = vk::ImageLayout::eShaderReadOnlyOptimal;
	vk::ImageLayout output = vk::ImageLayout::eGeneral;
};
bool DlssEvaluate(VkCommandBuffer command, const VulkanImage& color, vk::ImageView color_view, const VulkanImage& depth,
                  vk::ImageView depth_view, const VulkanImage& motion, vk::ImageView motion_view, const VulkanImage& output,
                  vk::ImageView output_view, vk::Extent2D render, vk::Extent2D upscale, const Camera& camera, bool reset,
                  bool taa, const DlssLayouts& layouts);
bool DlssUpscale(VkCommandBuffer command, const VulkanImage& color, vk::ImageView color_view, const VulkanImage& depth,
                 vk::ImageView depth_view, const VulkanImage& motion, vk::ImageView motion_view, const VulkanImage& output,
                 vk::ImageView output_view, vk::Extent2D render, vk::Extent2D upscale, const Camera& camera, bool reset) {
	return DlssEvaluate(command, color, color_view, depth, depth_view, motion, motion_view, output, output_view, render,
	                    upscale, camera, reset, false, DlssLayouts {});
}
bool DlssEvaluate(VkCommandBuffer command, const VulkanImage& color, vk::ImageView color_view, const VulkanImage& depth,
                  vk::ImageView depth_view, const VulkanImage& motion, vk::ImageView motion_view, const VulkanImage& output,
                  vk::ImageView output_view, vk::Extent2D render, vk::Extent2D upscale, const Camera& camera, bool reset,
                  bool taa, const DlssLayouts& layouts) {
	if (!g_sr_ready || g_api.dlss_set_options == nullptr || g_api.evaluate == nullptr) return false;
	std::lock_guard streamline_lock(g_streamline_mutex);
	const sl::ViewportHandle viewport {taa ? 2u : 1u};
	static vk::Extent2D      set_render[2] {}, set_output[2] {};
	const int                slot = taa ? 1 : 0;
	if (set_render[slot] != render || set_output[slot] != upscale) {
		const double ratio = static_cast<double>(upscale.height) / render.height;
		sl::DLSSOptions options {};
		// (NGX refuses DLSS's ultra quality mode, 0xbad00010 "unsupported parameter" on every evaluate: 1.3x renders
		// in the quality mode, whose render size range reaches up to the output size.)
		options.mode = ratio < 1.05   ? sl::DLSSMode::eDLAA
		               : ratio < 1.6  ? sl::DLSSMode::eMaxQuality
		               : ratio < 1.85 ? sl::DLSSMode::eBalanced
		               : ratio < 2.5  ? sl::DLSSMode::eMaxPerformance
		                              : sl::DLSSMode::eUltraPerformance;
		options.outputWidth     = upscale.width;
		options.outputHeight    = upscale.height;
		options.colorBuffersHDR = sl::Boolean::eTrue;
		options.useAutoExposure = sl::Boolean::eTrue;
		// KYTY_DLSS_PRESET=K (DLSS 4, transformer), L or M (DLSS 4.5), J; default: NVIDIA's choice per mode.
		sl::DLSSPreset preset = sl::DLSSPreset::eDefault;
		if (const char* text = std::getenv("KYTY_DLSS_PRESET"); text != nullptr && *text != '\0') {
			switch (*text) {
				case 'J': case 'j': preset = sl::DLSSPreset::ePresetJ; break;
				case 'K': case 'k': preset = sl::DLSSPreset::ePresetK; break;
				case 'L': case 'l': preset = sl::DLSSPreset::ePresetL; break;
				case 'M': case 'm': preset = sl::DLSSPreset::ePresetM; break;
				default: break;
			}
		}
		options.dlaaPreset = options.qualityPreset = options.balancedPreset = options.performancePreset =
		    options.ultraPerformancePreset = options.ultraQualityPreset = preset;
		sl::DLSSOptimalSettings supported {};
		const auto settings_result = g_api.dlss_get_optimal(options, supported);
		// Older/non-dynamic implementations may report only the optimal size.
		if (supported.renderWidthMin == 0 && supported.renderHeightMin == 0 &&
		    supported.renderWidthMax == 0 && supported.renderHeightMax == 0) {
			supported.renderWidthMin = supported.renderWidthMax = supported.optimalRenderWidth;
			supported.renderHeightMin = supported.renderHeightMax = supported.optimalRenderHeight;
		}
		if (settings_result != sl::Result::eOk ||
		    !SrRenderFits(render.width, render.height, supported.renderWidthMin, supported.renderHeightMin,
		                  supported.renderWidthMax, supported.renderHeightMax)) {
			std::printf("Upscaling: DLSS input %ux%u unsupported for mode %u (query %d, range %ux%u..%ux%u); "
			            "SR off, game scaler retained\n", render.width, render.height,
			            static_cast<unsigned>(options.mode), static_cast<int>(settings_result),
			            supported.renderWidthMin, supported.renderHeightMin,
			            supported.renderWidthMax, supported.renderHeightMax);
			std::fflush(stdout);
			g_sr_ready = false;
			return false;
		}
		std::printf("Upscaling: DLSS SDK optimal=%ux%u accepted=%ux%u..%ux%u\n",
		            supported.optimalRenderWidth, supported.optimalRenderHeight,
		            supported.renderWidthMin, supported.renderHeightMin,
		            supported.renderWidthMax, supported.renderHeightMax);
		if (const auto result = g_api.dlss_set_options(viewport, options); result != sl::Result::eOk) {
			std::printf("Upscaling: slDLSSSetOptions failed (%d): upscaling off\n", static_cast<int>(result));
			std::fflush(stdout);
			g_sr_ready = false;
			return false;
		}
		set_render[slot] = render;
		set_output[slot] = upscale;
		std::printf("Upscaling: DLSS %ux%u -> %ux%u, mode %u, preset %u\n", render.width, render.height, upscale.width,
		            upscale.height, static_cast<uint32_t>(options.mode), static_cast<uint32_t>(preset));
		std::fflush(stdout);
	}
	const uint32_t index = NextFrameId();
	sl::FrameToken* token = nullptr;
	if (NewFrameToken(token, &index) != sl::Result::eOk || token == nullptr) return false;
	auto constants = MakeSrConstants(camera.valid, camera, render);
	// After the game's TAA its jitter is resolved in the image; in its place the color is jittered (MakeSrConstants: the
	// camera's, KYTY_TAA_JITTER_SIGN=-1 the other way round).
	if (!taa) {
		constants.jitterOffset = sl::float2(0.0f, 0.0f);
	} else {
		static const float jitter_sign = [] {
			const char* value = std::getenv("KYTY_TAA_JITTER_SIGN");
			return value != nullptr && std::atof(value) < 0 ? -1.0f : 1.0f;
		}();
		constants.jitterOffset = sl::float2(jitter_sign * constants.jitterOffset.x, jitter_sign * constants.jitterOffset.y);
	}
	constants.reset        = reset ? sl::Boolean::eTrue : sl::Boolean::eFalse;
	if (!camera.valid) {
		// (No camera this frame: the vectors as they are, in UV units towards the motion.)
		static const float mv_sign = [] {
			const char* sign = std::getenv("KYTY_FRAMEGEN_MV_SIGN");
			return sign != nullptr && std::atof(sign) > 0 ? 1.0f : -1.0f;
		}();
		constants.mvecScale = sl::float2(mv_sign, mv_sign);
	}
	if (g_api.set_constants(constants, *token, viewport) != sl::Result::eOk) return false;
	const auto resource = [](const VulkanImage& image, vk::ImageView view, vk::ImageLayout layout) {
		sl::Resource r(sl::ResourceType::eTex2d, static_cast<VkImage>(image.image), nullptr, static_cast<VkImageView>(view),
		               static_cast<uint32_t>(layout));
		r.width        = image.extent.width;
		r.height       = image.extent.height;
		r.nativeFormat = static_cast<uint32_t>(image.format);
		r.mipLevels    = 1;
		r.arrayLayers  = 1;
		r.flags        = 0;
		r.usage        = static_cast<uint32_t>(static_cast<VkImageUsageFlags>(image.usage));
		return r;
	};
	auto in_color  = resource(color, color_view, layouts.color);
	auto in_depth  = resource(depth, depth_view, layouts.depth);
	auto in_motion = resource(motion, motion_view, layouts.motion);
	auto out_color = resource(output, output_view, layouts.output);
	const sl::Extent render_extent {0, 0, render.width, render.height};
	const sl::Extent output_extent {0, 0, upscale.width, upscale.height};
	const sl::ResourceTag tags[] {
	    sl::ResourceTag(&in_color, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &render_extent),
	    sl::ResourceTag(&in_depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilEvaluate, &render_extent),
	    sl::ResourceTag(&in_motion, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilEvaluate, &render_extent),
	    sl::ResourceTag(&out_color, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &output_extent),
	};
	auto* cmd = reinterpret_cast<sl::CommandBuffer*>(command);
	if (g_api.set_tag_for_frame(*token, viewport, tags, static_cast<uint32_t>(std::size(tags)), cmd) != sl::Result::eOk)
		return false;
	const sl::BaseStructure* inputs[] {&viewport};
	static uint32_t failures = 0;
	const auto result = g_api.evaluate(sl::kFeatureDLSS, *token, inputs, 1, cmd);
	// Streamline reports this warning only after a successful feature evaluation.
	// Keep its output and reset the failure streak rather than falling back or
	// disabling SR when the driver's memory budget is temporarily exhausted.
	if (result == sl::Result::eWarnOutOfVRAM) {
		static bool reported = false;
		if (!reported) {
			std::printf("Upscaling: DLSS evaluated successfully but exceeded the VRAM budget; continuing SR (memory pressure may cause stutter)\n");
			std::fflush(stdout);
			reported = true;
		}
	}
	if (result != sl::Result::eOk && result != sl::Result::eWarnOutOfVRAM) {
		if (failures < 4) {
			std::printf("Upscaling: DLSS evaluate failed (%d)\n", static_cast<int>(result));
			std::fflush(stdout);
		}
		// Each failed evaluate tries to create the NGX feature again (slow): off after 30 in a row.
		if (++failures >= 30) {
			std::printf("Upscaling: DLSS keeps failing: off (the game scales its 3D image itself)\n");
			std::fflush(stdout);
			g_sr_ready = false;
		}
		return false;
	}
	failures = 0;
	g_sr_frame_id = index;
	return true;
}
