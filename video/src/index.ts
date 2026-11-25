import { continueRender, delayRender, registerRoot } from 'remotion'
import { fontsReady } from './fonts'
import { RemotionRoot } from './Root'

const handle = delayRender('Loading self-hosted Geist fonts')
fontsReady.finally(() => continueRender(handle))

registerRoot(RemotionRoot)
