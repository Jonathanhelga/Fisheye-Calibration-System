// @ts-check
import {themes as prismThemes} from 'prism-react-renderer';

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: 'Calibration System Docs',
  tagline: 'Moil Fisheye Calibration System Documentation',
  favicon: 'img/favicon.ico',

  future: {
    v4: true,
  },

  url: 'https://perseverance-tech-tw.github.io',
  baseUrl: '/moilcalib_documentation/',

  organizationName: 'perseverance-tech-tw',
  projectName: 'moilcalib_documentation',

  onBrokenLinks: 'throw',
  onBrokenAnchors: 'ignore',

  markdown: {
    hooks: {
      onBrokenMarkdownLinks: 'warn',
    },
  },

  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      ({
        docs: {
          sidebarPath: './sidebars.js',
          editUrl: 'https://github.com/perseverance-tech-tw/moilcalib_documentation/tree/main/',
          // See VERSIONING.md at the repo root for the full folder map and
          // how to cut a new version with `npx docusaurus docs:version X`.
          lastVersion: '1.1', // the latest released version
          // The in-progress v2.0 docs in `docs/` are published again: the page
          // tree is in place and the application's HELP buttons already point at
          // /docs/v2.0/... (see cpp/app/Help.cpp), which 404s while this is off.
          // Set to `false` — and comment out the `current` entry below at the
          // same time — to hide v2.0 from the site again. Docusaurus rejects a
          // version key it isn't building, so the two must move together.
          includeCurrentVersion: true,
          versions: {
            // the in-progress draft, lives in `docs/`. Carries the default
            // "unreleased" banner pointing at lastVersion (v1.1).
            current: { label: 'v2.0 (ROS 2)', path: 'v2.0', badge: true },
            //
            // frozen snapshot, lives in versioned_docs/version-1.1/
            '1.1': {
              label: 'v1.1 (HTTP C++)',
              // Keep the /v1.1 URL it had while it was the current version —
              // its pages cross-link with absolute /docs/v1.1/... paths.
              // Explicit path keeps it here even as lastVersion, which would
              // otherwise be served at the /docs/ root.
              path: 'v1.1',
              banner: 'none',
            },
            // frozen snapshot, lives in versioned_docs/version-1.0/
            '1.0': {
              label: 'v1.0 (HTTP PYTHON)',
            },
          },
        },
        blog: {
          showReadingTime: true,
          feedOptions: {
            type: ['rss', 'atom'],
            xslt: true,
          },
          editUrl: 'https://github.com/perseverance-tech-tw/moilcalib_documentation/tree/main/blog/',
          onInlineTags: 'warn',
          onInlineAuthors: 'warn',
          onUntruncatedBlogPosts: 'warn',
        },
        theme: {
          customCss: './src/css/custom.css',
        },
      }),
    ],
  ],

  themeConfig:
    ({
      image: 'img/docusaurus-social-card.jpg',
      colorMode: {
        respectPrefersColorScheme: true,
      },
      navbar: {
        title: 'Calibration Docs',
        logo: {
          alt: 'Calibration Docs Logo',
          src: 'img/logo.svg',
        },
        items: [
          {
            type: 'docSidebar',
            sidebarId: 'tutorialSidebar',
            position: 'left',
            label: 'Documentation',
          },
          {
            to: '/blog',
            label: 'Blog',
            position: 'left',
          },
          {
            type: 'docsVersionDropdown',
            position: 'right',
          },
          {
            href: 'https://github.com/perseverance-tech-tw/moilcalib_documentation',
            label: 'GitHub',
            position: 'right',
          },
        ],
      },
      footer: {
        style: 'dark',
        links: [
          {
            title: 'Docs',
            items: [
              {
                label: 'Introduction',
                // Points at the latest released version (v1.1). No version is
                // served at the bare /docs/ root, because '1.1' pins path: 'v1.1'.
                to: '/docs/v1.1/intro',
              },
            ],
          },
          {
            title: 'More',
            items: [
              {
                label: 'Blog',
                to: '/blog',
              },
              {
                label: 'GitHub',
                href: 'https://github.com/perseverance-tech-tw/moilcalib_documentation',
              },
            ],
          },
        ],
        copyright: `Copyright © ${new Date().getFullYear()} Perseverance Technology. Built with Docusaurus.`,
      },
      prism: {
        theme: prismThemes.github,
        darkTheme: prismThemes.dracula,
      },
    }),
};

export default config;