import type { ReactNode } from 'react';
import clsx from 'clsx';
import Heading from '@theme/Heading';
import styles from './styles.module.css';

type FeatureItem = {
  title: string;
  Svg: React.ComponentType<React.ComponentProps<'svg'>>;
  description: ReactNode;
};

const FeatureList: FeatureItem[] = [
  {
    title: 'Decoupled Complexity',
    Svg: () => <div className={styles.placeholderIcon}>🌀</div>,
    description: (
      <>
        A dual-layer Euclidean engine separates melodic step progression from rhythmic triggering,
        enabling high-order syncopation that remains musically grounded.
      </>
    ),
  },
  {
    title: 'Modular Freedom',
    Svg: () => <div className={styles.placeholderIcon}>🕸️</div>,
    description: (
      <>
        No fixed signal path. Route any node to any other (acyclically) to create custom
        generative systems, logic-based switches, or parallel processing chains.
      </>
    ),
  },
  {
    title: 'Chord-First Workflow',
    Svg: () => <div className={styles.placeholderIcon}>🎹</div>,
    description: (
      <>
        Specialized logic for polyphonic performance. Extract top/bottom notes, stack octaves,
        or unzip chords into separate monophonic sequences.
      </>
    ),
  },
];

function Feature({ title, Svg, description }: FeatureItem) {
  return (
    <div className={clsx('col col--4')}>
      <div className="text--center">
        <Svg className={styles.featureSvg} role="img" />
      </div>
      <div className="text--center padding-horiz--md">
        <Heading as="h3">{title}</Heading>
        <p>{description}</p>
      </div>
    </div>
  );
}

export default function HomepageFeatures(): ReactNode {
  return (
    <section className={styles.features}>
      <div className="container">
        <div className="row">
          {FeatureList.map((props, idx) => (
            <Feature key={idx} {...props} />
          ))}
        </div>
      </div>
    </section>
  );
}
