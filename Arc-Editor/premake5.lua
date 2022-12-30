project "Arc-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"
	warnings "default"
	externalwarnings "off"
	rtti "off"
	postbuildmessage "================ Post-Build: Copying dependencies ================"

	binDir = "%{wks.location}/bin/" .. outputdir
	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp",

		"%{IncludeDir.glm}/util/glm.natvis",
		"%{IncludeDir.yaml_cpp}/../src/contrib/yaml-cpp.natvis",
		"%{IncludeDir.ImGui}/misc/debuggers/imgui.natvis",
	}

	includedirs
	{
		"src",
		"%{wks.location}/Arc/src",
		"%{wks.location}/Arc/vendor"
	}

	externalincludedirs
	{
		"%{wks.location}/Arc/vendor/spdlog/include",
		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.optick}",
		"%{IncludeDir.icons}",
	}

	libdirs
	{
		"%{LibDir.Mono}"
	}

	links
	{
		"Arc",
		"GLFW",
		"Glad",
		"ImGui",
		"yaml-cpp",
		"optick",
		"box2d",
		"JoltPhysics",
		"Arc-ScriptCore",
	}

	postbuildcommands
	{
		'{COPY} "../vendor" "%{binDir}"/vendor',
		'{COPY} "../Arc-Editor/assets" "%{cfg.targetdir}"/assets',
		'{COPY} "../Arc-Editor/mono" "%{cfg.targetdir}"/mono',
		'{COPY} "../Arc-Editor/Resources" "%{cfg.targetdir}"/Resources',
		'{COPY} "../Arc-Editor/imgui.ini" "%{cfg.targetdir}"',
	}

	filter "system:windows"
		systemversion "latest"
		links
		{
			"mono-2.0-sgen.lib",
			"opengl.dll"
		}

	filter "system:linux"
		pic "On"
		systemversion "latest"
		linkoptions { "`pkg-config --libs gtk+-3.0`" }
		links
		{
			"monosgen-2.0:shared",
			"GL:shared",
			"dl:shared"
		}

	filter "configurations:Debug"
		defines "ARC_DEBUG"
		runtime "Debug"
		symbols "on"
		postbuildcommands
		{
			'{COPY} "../Arc/vendor/mono/bin/Debug/mono-2.0-sgen.dll" "%{cfg.targetdir}"',
			'{COPY} "../Arc/vendor/mono/bin/Debug/libmonosgen-2.0.so" "%{cfg.targetdir}"',
		}

	filter "configurations:Release"
		defines "ARC_RELEASE"
		runtime "Release"
		optimize "speed"
		postbuildcommands
		{
			'{COPY} "../Arc/vendor/mono/bin/Release/mono-2.0-sgen.dll" "%{cfg.targetdir}"',
			'{COPY} "../Arc/vendor/mono/bin/Release/libmonosgen-2.0.so" "%{cfg.targetdir}"',
		}

	filter "configurations:Dist"
		defines "ARC_DIST"
		runtime "Release"
        optimize "speed"
		symbols "off"
		postbuildcommands
		{
			'{COPY} "../Arc/vendor/mono/bin/Release/mono-2.0-sgen.dll" "%{cfg.targetdir}"',
			'{COPY} "../Arc/vendor/mono/bin/Release/libmonosgen-2.0.so" "%{cfg.targetdir}"',
		}
