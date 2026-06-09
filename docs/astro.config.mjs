import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

export default defineConfig({
  site: 'https://suleymanlaarabi.github.io',
  base: '/sireflect',
  integrations: [
    starlight({
      title: 'Sireflect',
      description: 'Documentation for the Sireflect C reflection library.',
      sidebar: [
        { label: 'Overview', link: '/' },
        {
          label: 'Guide',
          items: [
            'getting-started',
            'supported-syntax',
            'type-metadata',
            'field-access',
            'limits',
          ],
        },
        {
          label: 'Reference',
          items: ['reference/api'],
        },
      ],
    }),
  ],
});
