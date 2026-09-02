import React from 'react';
import styles from './styles.module.css';

/**
 * Reserved space for a screenshot that has not been captured yet.
 *
 * Renders a dashed placeholder of roughly the height the real image will take,
 * naming the file to save and describing exactly what to capture. It holds no
 * <img>, so the page builds while the file does not exist yet — a markdown
 * image pointing at a missing file fails the webpack build instead.
 *
 *   <Shot
 *     id="fig-2"
 *     number="2"
 *     file="main-window-axis-panel.png"
 *     caption="Axis Control Panel."
 *     what="Open the app connected to the rig. Capture the Axis Control Panel only."
 *     height={420}
 *   />
 *
 * To swap in the real capture, save the file into `assets/images/` and replace
 * the whole block with a <Figure> carrying the same id, number and caption:
 *
 *   <Figure id="fig-2" number="2" caption="Axis Control Panel.">
 *
 *   ![Axis Control Panel](../assets/images/main-window-axis-panel.png)
 *
 *   </Figure>
 */
export default function Shot({id, number, file, caption, what, height = 320}) {
  const label = (
    <strong>Figure {number}.</strong>
  );

  return (
    <figure className={styles.shot} id={id}>
      <div className={styles.box} style={{minHeight: `${height}px`}}>
        <div className={styles.badge}>SCREENSHOT NEEDED</div>
        {file && <div className={styles.file}>assets/images/{file}</div>}
        {what && <div className={styles.what}>{what}</div>}
      </div>
      <figcaption className={styles.caption}>
        {number != null && (
          <>
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
    </figure>
  );
}
