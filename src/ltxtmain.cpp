
#include "common.hpp"
#include "filebuf.hpp"
#include "ddstxt16.hpp"
#include "ba2file.hpp"
#include "esmfile.hpp"
#include "landdata.hpp"
#include "landtxt.hpp"
#include "downsamp.hpp"

#include <thread>

static bool checkNameExtension(const char *fileName, const char *suffix)
{
  unsigned int  n = 0U;
  unsigned int  s = 0U;
  size_t  len;
  if (fileName && *fileName && (len = std::strlen(fileName)) >= 4)
    n = FileBuffer::readUInt32Fast(fileName + (len - 4));
  if (suffix && *suffix && (len = std::strlen(suffix)) >= 4)
    s = FileBuffer::readUInt32Fast(suffix + (len - 4));
  n = n | ((n & 0x40404000U) >> 1);
  s = s | ((s & 0x40404000U) >> 1);
  return (n == s);
}

static void loadTextures(
    std::vector< LandscapeTextureSet >& textures,
    const char *listFileName, const LandscapeData *landData,
    bool verboseMode, int mipOffset = 0, const BA2File *ba2File = 0)
{
  textures.clear();
  std::vector< std::string >  fileNames;
  std::vector< std::string >  fileNames2;
  if (!landData)
  {
    FileBuffer  inFile(listFileName);
    std::string s;
    while (true)
    {
      unsigned char c = '\n';
      if (inFile.getPosition() < inFile.size())
        c = inFile.readUInt8();
      if (c == '\t')
      {
        if (!checkNameExtension(s.c_str(), ".dds"))
        {
          s.clear();
          continue;
        }
        c = '\n';
      }
      if (c == '\r' || c == '\n')
      {
        while (s.length() > 0 && s.back() == ' ')
          s.resize(s.length() - 1);
        if (!s.empty())
        {
          fileNames.push_back(s);
          fileNames2.emplace_back();
          s.clear();
        }
      }
      if (inFile.getPosition() >= inFile.size())
        break;
      if (c > (unsigned char) ' ' || (c == ' ' && !s.empty()))
        s += char(c);
    }
  }
  else
  {
    for (size_t i = 0; i < landData->getTextureCount(); i++)
    {
      const CE2Material *m = landData->getTextureMaterial(i);
      const std::string *s0 = (std::string *) 0;
      const std::string *s2 = (std::string *) 0;
      if (m && m->layerMask)
      {
        const CE2Material::Layer  *l =
            m->layers[std::countr_zero(m->layerMask)];
        if (l && l->material && l->material->textureSet)
        {
          if (l->material->textureSet->texturePathMask & 1U)
            s0 = l->material->textureSet->texturePaths[0];
          if (l->material->textureSet->texturePathMask & 4U)
            s2 = l->material->textureSet->texturePaths[2];
        }
      }
      if (!s0)
        fileNames.emplace_back();
      else
        fileNames.push_back(*s0);
      if (!s2)
        fileNames2.emplace_back();
      else
        fileNames2.push_back(*s2);
    }
  }
  if (fileNames.size() < 1)
    errorMessage("texture file list is empty");
  textures.resize(fileNames.size());
  std::vector< unsigned char >  tmpBuf;
  for (size_t i = 0; i < fileNames.size(); i++)
  {
    for (size_t j = 0; j <= 2; j = j + 2)
    {
      const std::string&  fileName = (j == 0 ? fileNames[i] : fileNames2[i]);
      if (fileName.empty())
        continue;
      try
      {
        if (verboseMode)
        {
          std::fprintf(stderr, "\rLoading texture %3d: %-58s",
                       int(i), fileName.c_str());
        }
        if (ba2File)
        {
          int     n = ba2File->extractTexture(tmpBuf, fileName, mipOffset);
          textures[i][j] = new DDSTexture16(tmpBuf.data(), tmpBuf.size(), n);
        }
        else
        {
          textures[i][j] = new DDSTexture16(fileName.c_str(), mipOffset);
        }
      }
      catch (...)
      {
        if (checkNameExtension(fileName.c_str(), ".dds") && !(ba2File && i))
        {
          if (verboseMode)
            std::fputc('\n', stderr);
          throw;
        }
      }
    }
  }
  if (verboseMode)
    std::fputc('\n', stderr);
}

class RenderThread : public LandscapeTexture
{
 public:
  std::thread *threadPtr;
  int     xyScale;
  std::vector< unsigned char >  outBuf;
  RenderThread(const unsigned char *txtSetPtr, const unsigned char *ltex16Ptr,
               int vertexCntX, int vertexCntY, int cellResolution,
               const LandscapeTextureSet *landTxts, size_t landTxtCnt);
  RenderThread(const LandscapeData& landData,
               const LandscapeTextureSet *landTxts, size_t landTxtCnt);
  virtual ~RenderThread();
  void renderLines(int y0, int y1);
  static void runThread(RenderThread *p, int y0, int y1);
};

RenderThread::RenderThread(
    const unsigned char *txtSetPtr, const unsigned char *ltex16Ptr,
    int vertexCntX, int vertexCntY, int cellResolution,
    const LandscapeTextureSet *landTxts, size_t landTxtCnt)
  : LandscapeTexture(txtSetPtr, ltex16Ptr, vertexCntX, vertexCntY,
                     cellResolution, landTxts, landTxtCnt),
    threadPtr((std::thread *) 0),
    xyScale(0)
{
}

RenderThread::RenderThread(
    const LandscapeData& landData,
    const LandscapeTextureSet *landTxts, size_t landTxtCnt)
  : LandscapeTexture(landData, landTxts, landTxtCnt),
    threadPtr((std::thread *) 0),
    xyScale(0)
{
}

RenderThread::~RenderThread()
{
  if (threadPtr)
  {
    threadPtr->join();
    delete threadPtr;
  }
}

void RenderThread::renderLines(int y0, int y1)
{
  outBuf.resize(size_t(width << xyScale) * size_t(y1 - y0) * 4U);
  unsigned char *outBufs[8];
  for (size_t i = 0; i < 8; i++)
    outBufs[i] = (unsigned char *) 0;
  outBufs[0] = outBuf.data();
  renderTexture(outBufs, xyScale,
                0, y0 >> xyScale, width - 1, (y1 >> xyScale) - 1);
}

void RenderThread::runThread(RenderThread *p, int y0, int y1)
{
  p->renderLines(y0, y1);
}

static const char *usageStrings[] =
{
  "Usage:",
  "    landtxt INFILE.DDS OUTFILE.DDS TXTSET.DDS LTEXLIST.TXT [OPTIONS...]",
  "    landtxt INFILE.ESM[,...] OUTFILE.DDS [OPTIONS...]",
  "",
  "General options:",
  "    --                  remaining options are file names",
  "    -d PATHNAME         game data path or texture archive file",
  "    -mip FLOAT          texture mip level",
  "    -mult FLOAT         multiply RGB output with the specified value",
  "    -defclr 0x00RRGGBB  default color for untextured areas",
  "    -scale INT          scale output resolution by 2^N",
  "    -threads INT        set the number of threads to use",
  "    -ssaa INT           render at 2^N resolution and downsample",
  "    -q                  do not print texture file names",
  "",
  "ESM/BTD input file options:",
  "    -btd FILENAME.BTD   read terrain data from Starfield .btd file",
  "    -w FORMID           form ID of world to use from ESM input file",
  "    -r X0 Y0 X1 Y1      limit range of cells to X0,Y0 (SW) to X1,Y1 (NE)",
  "    -l INT              level of detail to use from BTD file (0 to 4)",
  (char *) 0
};

static bool archiveFilterFunction(void *p, const std::string_view& s)
{
  (void) p;
  if (!(s.ends_with(".dds") || s.ends_with(".cdb") || s.ends_with(".mat") ||
        s.ends_with(".btd")))
  {
    return false;
  }
  return (s.find("/lod/") == std::string_view::npos &&
          s.find("/actors/") == std::string_view::npos);
}

int main(int argc, char **argv)
{
  std::vector< LandscapeTextureSet >  landTextures;
  std::vector< RenderThread * > threads;
  DDSInputFile  *inFile = (DDSInputFile *) 0;
  DDSInputFile  *txtSetFile = (DDSInputFile *) 0;
  ESMFile       *esmFile = (ESMFile *) 0;
  BA2File       *ba2File = (BA2File *) 0;
  CE2MaterialDB *materials = (CE2MaterialDB *) 0;
  LandscapeData *landData = (LandscapeData *) 0;
  int     err = 1;
  try
  {
    float         mipLevel = 5.0f;
    float         rgbScale = 1.0f;
    std::uint32_t defaultColor = 0x003F3F3F;
    int           xyScale = 0;
    int           threadCnt = int(std::thread::hardware_concurrency());
    threadCnt = (threadCnt > 1 ? (threadCnt < 256 ? threadCnt : 256) : 1);
    bool          verboseMode = true;
    unsigned char txtSetMip = 0;
    const char    *archivePath = (char *) 0;
    const char    *btdFileName = (char *) 0;
    unsigned int  worldFormID = 0U;
    int           xMin = -32768;
    int           yMin = -32768;
    int           xMax = 32767;
    int           yMax = 32767;
    unsigned char btdLOD = 2;
    unsigned char ssaaLevel = 0;
    unsigned int  hdrBuf[11];

    std::vector< const char * > args;
    for (int i = 1; i < argc; i++)
    {
      if (argv[i][0] != '-')
      {
        args.push_back(argv[i]);
        continue;
      }
      if (std::strcmp(argv[i], "--") == 0)
      {
        while (++i < argc)
          args.push_back(argv[i]);
        break;
      }
      if (std::strcmp(argv[i], "-h") == 0 ||
          std::strcmp(argv[i], "--help") == 0)
      {
        args.clear();
        err = 0;
        break;
      }
      if (std::strcmp(argv[i], "-d") == 0)
      {
        if (++i >= argc)
          throw FO76UtilsError("missing argument for %s", argv[i - 1]);
        archivePath = argv[i];
      }
      else if (std::strcmp(argv[i], "-mip") == 0)
      {
        if (++i >= argc)
          throw FO76UtilsError("missing argument for %s", argv[i - 1]);
        mipLevel = float(parseFloat(argv[i], "invalid mip level", 0.0, 16.0));
      }
      else if (std::strcmp(argv[i], "-mult") == 0)
      {
        if (++i >= argc)
          throw FO76UtilsError("missing argument for %s", argv[i - 1]);
        rgbScale = float(parseFloat(argv[i], "invalid RGB output multiplier",
                                    0.5, 8.0));
      }
      else if (std::strcmp(argv[i], "-defclr") == 0)
      {
        if (++i >= argc)
          throw FO76UtilsError("missing argument for %s", argv[i - 1]);
        defaultColor =
            (std::uint32_t) parseInteger(argv[i], 0, "invalid default color",
                                         0L, 0x00FFFFFFL);
      }
      else if (std::strcmp(argv[i], "-scale") == 0)
      {
        if (++i >= argc)
          throw FO76UtilsError("missing argument for %s", argv[i - 1]);
        xyScale = int(parseInteger(argv[i], 10, "invalid scale", 0, 6));
      }
      else if (std::strcmp(argv[i], "-threads") == 0)
      {
        if (++i >= argc)
          throw FO76UtilsError("missing argument for %s", argv[i - 1]);
        threadCnt =
            int(parseInteger(argv[i], 10, "invalid thread count", 1, 64));
      }
      else if (std::strcmp(argv[i], "-ssaa") == 0)
      {
        if (++i >= argc)
          throw FO76UtilsError("missing argument for %s", argv[i - 1]);
        ssaaLevel =
            (unsigned char) parseInteger(argv[i], 0,
                                         "invalid argument for -ssaa", 0, 2);
      }
      else if (std::strcmp(argv[i], "-q") == 0)
      {
        verboseMode = false;
      }
      else if (std::strcmp(argv[i], "-btd") == 0)
      {
        if (++i >= argc)
          throw FO76UtilsError("missing argument for %s", argv[i - 1]);
        btdFileName = argv[i];
      }
      else if (std::strcmp(argv[i], "-w") == 0)
      {
        if (++i >= argc)
          throw FO76UtilsError("missing argument for %s", argv[i - 1]);
        worldFormID =
            (unsigned int) parseInteger(argv[i], 0, "invalid world form ID",
                                        0L, 0x0FFFFFFFL);
      }
      else if (std::strcmp(argv[i], "-r") == 0)
      {
        if ((i + 4) >= argc)
          throw FO76UtilsError("missing argument for %s", argv[i]);
        xMin = int(parseInteger(argv[++i], 10, "invalid xMin", -32768, 32767));
        yMin = int(parseInteger(argv[++i], 10, "invalid yMin", -32768, 32767));
        xMax = int(parseInteger(argv[++i], 10, "invalid xMax", xMin, 32767));
        yMax = int(parseInteger(argv[++i], 10, "invalid yMax", yMin, 32767));
      }
      else if (std::strcmp(argv[i], "-l") == 0)
      {
        if (++i >= argc)
          throw FO76UtilsError("missing argument for %s", argv[i - 1]);
        btdLOD =
            (unsigned char) parseInteger(argv[i], 10, "invalid level of detail",
                                         0, 4);
      }
      else
      {
        throw FO76UtilsError("invalid command line option: %s", argv[i]);
      }
    }
    if (args.size() < 1 ||
        args.size() != (checkNameExtension(args[0], ".esm") ? 2 : 4))
    {
      for (size_t i = 0; usageStrings[i]; i++)
        std::fprintf(stderr, "%s\n", usageStrings[i]);
      return err;
    }
    threads.resize(size_t(threadCnt), (RenderThread *) 0);

    if (archivePath)
    {
      ba2File = new BA2File(archivePath, &archiveFilterFunction);
      materials = new CE2MaterialDB();
      materials->loadArchives(*ba2File);
    }
    int     width = 0;
    int     height = 0;
    if (ssaaLevel > 0)
      mipLevel = std::max(mipLevel - float(int(ssaaLevel)), 0.0f);
    if (!checkNameExtension(args[0], ".esm"))
    {
      // DDS input files
      int     pixelFormat = 0;
      inFile = new DDSInputFile(args[0], width, height, pixelFormat, hdrBuf);
      if (!(pixelFormat == DDSInputFile::pixelFormatA16 ||
            pixelFormat == DDSInputFile::pixelFormatGRAY16 ||
            pixelFormat == DDSInputFile::pixelFormatRGBA16 ||
            pixelFormat == (DDSInputFile::pixelFormatUnknown + 16)))
      {
        errorMessage("invalid input file pixel format");
      }
      int     tmpWidth = 0;
      int     tmpHeight = 0;
      int     tmpPixelFormat = 0;
      txtSetFile = new DDSInputFile(args[2],
                                    tmpWidth, tmpHeight, tmpPixelFormat);
      if (!(tmpPixelFormat == DDSInputFile::pixelFormatR8 ||
            tmpPixelFormat == DDSInputFile::pixelFormatGRAY8))
      {
        errorMessage("invalid texture set file pixel format");
      }
      while (!((width << 4) == (tmpWidth << txtSetMip) &&
               height == (tmpHeight << txtSetMip)))
      {
        if (++txtSetMip >= 8)
          errorMessage("texture set dimensions do not match input file");
      }
      loadTextures(landTextures, args[3], (LandscapeData *) 0, verboseMode,
                   int(mipLevel), ba2File);
    }
    else
    {
      // ESM/BTD input file(s)
      esmFile = new ESMFile(args[0]);
      landData = new LandscapeData(esmFile, btdFileName, ba2File, materials,
                                   worldFormID, btdLOD, xMin, yMin, xMax, yMax);
      while ((2 << txtSetMip) < landData->getCellResolution())
        txtSetMip++;
      width = landData->getImageWidth();
      height = landData->getImageHeight();
      hdrBuf[0] = 0x5F324543U;          // "CE2_"
      hdrBuf[1] = 0x444E414CU;          // "LAND"
      hdrBuf[2] = (unsigned int) landData->getXMin();
      hdrBuf[3] = (unsigned int) landData->getYMin();
      hdrBuf[4] = (unsigned int) roundFloat(landData->getZMin());
      hdrBuf[5] = (unsigned int) landData->getXMax();
      hdrBuf[6] = (unsigned int) landData->getYMax();
      hdrBuf[7] = (unsigned int) roundFloat(landData->getZMax());
      hdrBuf[8] = (unsigned int) roundFloat(landData->getWaterLevel());
      hdrBuf[9] = (unsigned int) landData->getCellResolution();
      hdrBuf[10] = 0U;
      loadTextures(landTextures, (char *) 0, landData, verboseMode,
                   int(mipLevel), ba2File);
    }
    mipLevel = mipLevel - float(int(mipLevel));

    width = width << xyScale;
    height = height << xyScale;
    hdrBuf[9] = hdrBuf[9] << xyScale;
    DDSOutputFile outFile(args[1],
                          width, height, DDSInputFile::pixelFormatRGB24,
                          hdrBuf, 16384);
    if (ssaaLevel > 0)
    {
      xyScale += int(ssaaLevel);
      width = width << ssaaLevel;
      height = height << ssaaLevel;
    }
    for (size_t i = 0; i < threads.size(); i++)
    {
      if (!landData)
      {
        threads[i] = new RenderThread(txtSetFile->data(), inFile->data(),
                                      width >> xyScale, height >> xyScale,
                                      2 << txtSetMip, landTextures.data(),
                                      landTextures.size());
      }
      else
      {
        threads[i] = new RenderThread(*landData, landTextures.data(),
                                      landTextures.size());
      }
      threads[i]->setMipLevel(mipLevel);
      threads[i]->setRGBScale(rgbScale);
      threads[i]->setDefaultColor(defaultColor);
      threads[i]->xyScale = xyScale;
    }
    std::vector< std::uint32_t >  downsampleBuf;
    std::uint32_t *lineBuf = (std::uint32_t *) 0;
    int     h = 1 << xyScale;
    h = (h > 32 ? h : 32);
    if (ssaaLevel > 0)
    {
      downsampleBuf.resize(size_t(width) * size_t(h * 3)
                           + size_t(width >> ssaaLevel));
      lineBuf = downsampleBuf.data() + (size_t(width) * size_t(h * 3));
    }
    int     downsampleY0 = 0;
    int     downsampleY1 = 0;
    for (int y = 0; y < height; )
    {
      for (size_t i = 0; i < threads.size() && y < height; i++)
      {
        int     nextY = y + h;
        if (nextY > height)
          nextY = height;
        threads[i]->threadPtr = new std::thread(RenderThread::runThread,
                                                threads[i], y, nextY);
        y = nextY;
      }
      for (size_t i = 0; i < threads.size() && threads[i]->threadPtr; i++)
      {
        threads[i]->threadPtr->join();
        delete threads[i]->threadPtr;
        threads[i]->threadPtr = (std::thread *) 0;
        if (!ssaaLevel)
        {
          for (size_t j = 0; (j + 4) <= threads[i]->outBuf.size(); j = j + 4)
          {
            std::uint32_t c =
                FileBuffer::readUInt32Fast(threads[i]->outBuf.data() + j);
            unsigned char tmpBuf[3];
            tmpBuf[0] = (unsigned char) ((c >> 16) & 0xFF);     // B
            tmpBuf[1] = (unsigned char) ((c >> 8) & 0xFF);      // G
            tmpBuf[2] = (unsigned char) (c & 0xFF);             // R
            outFile.writeData(tmpBuf, 3);
          }
        }
        else
        {
          size_t  k = size_t(downsampleY1) * size_t(width);
          downsampleY1 += int(threads[i]->outBuf.size() / (size_t(width) * 4U));
          for (size_t j = 0; (j + 4) <= threads[i]->outBuf.size(); j = j + 4)
          {
            downsampleBuf[k] =
                FileBuffer::readUInt32Fast(threads[i]->outBuf.data() + j);
            k++;
          }
          bool    endFlag =
              (y >= height &&
               !((i + 1) < threads.size() && threads[i + 1]->threadPtr));
          if (downsampleY1 >= (h * 2) || endFlag)
          {
            std::uint32_t *p = downsampleBuf.data();
            for (int yc = downsampleY0;
                 yc < (downsampleY1 - (!endFlag ? 16 : 0));
                 yc = yc + (1 << ssaaLevel))
            {
              if (ssaaLevel == 1)
                downsample2xFilter_Line(lineBuf, p, width, downsampleY1, yc, 0);
              else
                downsample4xFilter_Line(lineBuf, p, width, downsampleY1, yc, 0);
              for (int xc = 0; xc < (width >> ssaaLevel); xc++)
              {
                std::uint32_t c = lineBuf[xc];
                outFile.writeByte((unsigned char) ((c >> 16) & 0xFF));
                outFile.writeByte((unsigned char) ((c >> 8) & 0xFF));
                outFile.writeByte((unsigned char) (c & 0xFF));
              }
            }
            if (!endFlag)
            {
              std::memcpy(p, p + (size_t(downsampleY1 - 32) * size_t(width)),
                          (size_t(width) * 32U) * sizeof(std::uint32_t));
              downsampleY0 = 16;
              downsampleY1 = 32;
            }
          }
        }
      }
    }
    err = 0;
  }
  catch (std::exception& e)
  {
    std::fprintf(stderr, "landtxt: %s\n", e.what());
    err = 1;
  }
  for (size_t i = 0; i < threads.size(); i++)
  {
    if (threads[i])
      delete threads[i];
  }
  for (size_t i = 0; i < landTextures.size(); i++)
  {
    if (landTextures[i][0])
      delete const_cast< DDSTexture16 * >(landTextures[i][0]);
  }
  if (landData)
    delete landData;
  if (esmFile)
    delete esmFile;
  if (txtSetFile)
    delete txtSetFile;
  if (inFile)
    delete inFile;
  if (materials)
    delete materials;
  if (ba2File)
    delete ba2File;
  return err;
}

