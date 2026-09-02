import React from 'react';
import styles from './styles.module.css';

/**
 * Captioned figure.
 *
 * The image must be passed as a markdown image in the children, not as a src
 * prop. Docusaurus only rewrites markdown `image` nodes to webpack requires
 * (see @docusaurus/mdx-loader remark/transformImage), so a string src would
 * ship an unresolved relative path.
 *
 *   <Figure id="fig-1" number="1" caption="Main window overview.">
 *
 *   ![Main window overview](../assets/images/img_12.png)
 *
 *   </Figure>
 *
 * `caption` accepts a plain string or JSX for captions that need inline markup.
 */
export default function Figure({id, number, title, caption, children}) {
  const hasCaption = number != null || caption != null;
  // "Figure 3." or, when the figure is titled, "Figure 3. Result Table View."
  const label = (
    <strong>
      Figure {number}.{title ? ` ${title}.` : ''}
    </strong>
  );

  return (
    <figure className={styles.figure} id={id}>
      {children}
      {hasCaption && (
        <figcaption className={styles.caption}>
          {number != null && (
            <>
              {/* Only a figure with an id can be linked to, so only that one
                  gets a self-link. Otherwise render plain text. */}
              {id ? (
                <a href={`#${id}`} className={styles.number}>
                  {label}
                </a>
              ) : (
                label
              )}{' '}
            </>
          )}
          {caption}
        </figcaption>
      )}
    </figure>
  );
}
