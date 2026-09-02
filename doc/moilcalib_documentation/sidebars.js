// Sidebar for the CURRENT (unreleased v2.0, ROS 2) docs in `docs/`.
//
// The order follows the actual working flow: install -> confirm the rig is
// reachable -> learn the main window -> run the calibration (pattern -> capture
// -> result) -> verify the camera parameters -> manage stored data.
//
// The "ROS 2 Rig Connection" category is new in v2.0 and sits before the main
// window on purpose: in v1.1 you typed three URLs into that window, and in v2.0
// there is nothing to type, so a client that cannot see the rig looks exactly
// like a working one until you try to capture.
//
// CONVENTION: every label lives in this file. Pages must NOT set `sidebar_label`
// in their frontmatter. Docusaurus resolves a doc label as
// `frontMatter.sidebar_label ?? item.label ?? title` (see
// plugin-content-docs/lib/props.js), so any frontmatter label silently wins over
// the `label` below and the step numbers drift out of sync.
const sidebars = {
  tutorialSidebar: [
    { type: 'doc', id: 'intro', label: 'Introduction' },

    {
      type: 'category',
      label: 'Installation',
      collapsed: true,
      items: [
        { type: 'doc', id: 'installation/server', label: 'Server Installation (ROS 2)' },
        {
          type: 'category',
          label: 'Client Installation Guide',
          collapsed: false,
          link: {
            type: 'doc',
            id: 'installation/client-installation-guide',
          },
          items: [
            {
              type: 'doc',
              id: 'installation/client-windows',
              label: 'Windows (Installer)',
            },
            {
              type: 'doc',
              id: 'installation/client-linux',
              label: 'Linux (Ubuntu 24.04)',
            },
            { type: 'doc', id: 'installation/client-docker', label: 'Docker' },
            {
              type: 'doc',
              id: 'installation/client-windows-native',
              label: 'Windows (Native MSVC)',
            },
          ],
        },
      ],
    },

    {
      type: 'category',
      label: 'ROS 2 Rig Connection',
      collapsed: false,
      items: [
        { type: 'doc', id: 'ros/ros-architecture', label: 'ROS 2 Architecture' },
        { type: 'doc', id: 'ros/connect-to-rig', label: 'Connect the App to the Rig' },
      ],
    },

    {
      type: 'doc',
      id: 'system-overview/main-window',
      label: 'Main Window Reference',
    },

    {
      type: 'category',
      label: 'Calibration Workflow',
      collapsed: false,
      items: [
        {
          type: 'category',
          label: '1. Pattern Setup',
          collapsed: false,
          items: [
            {
              type: 'doc',
              id: 'calibration/pct-pattern-generator',
              label: 'PCT Pattern Generator',
            },
            {
              type: 'doc',
              id: 'calibration/monitor-viewer',
              label: 'Monitor Viewer',
            },
          ],
        },

        {
          type: 'doc',
          id: 'calibration/camera-calibration',
          label: '2. Camera Calibration',
        },

        {
          type: 'category',
          label: '3. Calibration Result',
          collapsed: false,
          link: {
            type: 'doc',
            id: 'calibration/cali-result/index',
          },
          items: [
            {
              type: 'doc',
              id: 'calibration/cali-result/main-window-overview',
              label: 'Main Window Overview',
            },
            {
              type: 'doc',
              id: 'calibration/cali-result/result-table-view',
              label: 'Result Table View',
            },
            {
              type: 'doc',
              id: 'calibration/cali-result/parameter-view',
              label: 'Parameter View',
            },
            {
              type: 'doc',
              id: 'calibration/cali-result/overlap-and-aggregation-view',
              label: 'Overlap & Aggregation View',
            },
            {
              type: 'doc',
              id: 'calibration/cali-result/entrance-pupil-analysis',
              label: 'Entrance-Pupil Analysis',
            },
            {
              type: 'doc',
              id: 'calibration/cali-result/pct-recommend',
              label: 'PCT Recommend',
            },
          ],
        },

        {
          type: 'doc',
          id: 'calibration/reload-calibration-data',
          label: '4. Reload Calibration Data',
        },

        {
          type: 'doc',
          id: 'verification/setup-center',
          label: '5. Setup Center',
        },

        {
          type: 'doc',
          id: 'verification/3d-verification',
          label: '6. 3D Verification',
        },
      ],
    },

    {
      type: 'category',
      label: 'Database',
      collapsed: true,
      link: {
        type: 'doc',
        id: 'database/database-overview',
      },
      items: [
        {
          type: 'doc',
          id: 'database/load-to-system',
          label: 'Load to System (SharePoint)',
        },
      ],
    },

    {
      type: 'category',
      label: 'Development',
      collapsed: true,
      items: [
        {
          type: 'doc',
          id: 'development/codebase-overview',
          label: 'Codebase Overview',
        },
      ],
    },
  ],
};

export default sidebars;
