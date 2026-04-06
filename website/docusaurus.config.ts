import { themes as prismThemes } from 'prism-react-renderer';
import type { Config } from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';

// This runs in Node.js - Don't use client-side code here (browser APIs, JSX...)

const config: Config = {
  title: 'Arps Euclidya',
  tagline: 'Modular, node-based MIDI orchestration',
  favicon: 'img/logo.ico',

  // Future flags, see https://docusaurus.io/docs/api/docusaurus-config#future
  future: {
    v4: true, // Improve compatibility with the upcoming Docusaurus v4
  },

  // Set the production url of your site here
  url: 'https://arps.chalkwalkmusic.com',
  baseUrl: '/',

  // GitHub pages deployment config.
  // If you aren't using GitHub pages, you don't need these.
  organizationName: 'chalkwalk', // Usually your GitHub org/user name.
  projectName: 'arps-euclidya', // Usually your repo name.

  onBrokenLinks: 'throw',

  // Even if you don't use internationalization, you can use this field to set
  // useful metadata like html lang. For example, if your site is Chinese, you
  // may want to replace "en" with "zh-Hans".
  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      {
        docs: {
          sidebarPath: './sidebars.ts',
          // Please change this to your repo.
          // Remove this to remove the "edit this page" links.
          editUrl:
            'https://github.com/chalkwalk/arps-euclidya/tree/main/website/',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      } satisfies Preset.Options,
    ],
  ],

  themeConfig: {
    // Replace with your project's social card
    image: 'img/social-card.png',
    colorMode: {
      respectPrefersColorScheme: true,
    },
    navbar: {
      title: 'Arps Euclidya',
      logo: {
        alt: 'Arps Euclidya Logo',
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
          href: 'https://github.com/chalkwalk/arps-euclidya',
          label: 'GitHub',
          position: 'right',
        },
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'Documentation',
          items: [
            {
              label: 'Introduction',
              to: '/docs/introduction',
            },
            {
              label: 'Node Dictionary',
              to: '/docs/node-dictionary',
            },
            {
              label: 'Tutorials',
              to: '/docs/tutorials',
            },
          ],
        },
        {
          title: 'ChalkWalk',
          items: [
            {
              label: 'ChalkWalk Music',
              href: 'https://chalkwalkmusic.com/',
            },
            {
              label: 'YouTube',
              href: 'https://www.youtube.com/chalkwalkmusic',
            },
            {
              label: 'GitHub',
              href: 'https://github.com/chalkwalk/arps-euclidya',
            },
          ],
        },
      ],
      copyright: `Copyright © ${new Date().getFullYear()} ChalkWalk. Built with Docusaurus.`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.dracula,
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
