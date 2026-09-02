import MDXComponents from '@theme-original/MDXComponents';
import Figure from '@site/src/components/Figure';
import Shot from '@site/src/components/Shot';

export default {
  ...MDXComponents,
  // Available in every .md/.mdx file without an import.
  Figure,
  // Reserved space for a screenshot not captured yet. Swap for <Figure> once
  // the file exists — see src/components/Shot/index.js.
  Shot,
};
