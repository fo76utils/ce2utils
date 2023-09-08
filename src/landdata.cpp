
#include "common.hpp"
#include "landdata.hpp"

void LandscapeData::allocateDataBuf(unsigned int formatMask, bool isFO76)
{
  size_t  hmapDataSize = 0;
  size_t  ltexData32Size = 0;
  size_t  vnmlDataSize = 0;
  size_t  vclrData24Size = 0;
  size_t  ltexData16Size = 0;
  size_t  txtSetDataSize = 0;
  size_t  vertexCnt = size_t(getImageWidth()) * size_t(getImageHeight());
  if (formatMask & 0x01)
    hmapDataSize = vertexCnt * sizeof(std::uint16_t);
  if (!isFO76)
  {
    if (formatMask & 0x02)
      ltexData32Size = vertexCnt * sizeof(std::uint32_t);
    if (formatMask & 0x04)
      vnmlDataSize = vertexCnt * 3;
    if (formatMask & 0x08)
      vclrData24Size = vertexCnt * 3;
  }
  else
  {
    if (formatMask & 0x02)
      ltexData16Size = vertexCnt * sizeof(std::uint16_t);
  }
  if (formatMask & 0x02)
  {
    txtSetDataSize =
        size_t(cellMaxX + 1 - cellMinX) * size_t(cellMaxY + 1 - cellMinY) * 64;
  }
  size_t  totalDataSize =
      (hmapDataSize + ltexData32Size + vnmlDataSize + vclrData24Size
       + ltexData16Size + txtSetDataSize)
      / sizeof(std::uint32_t);
  if (totalDataSize < 1)
    return;
  dataBuf.resize(totalDataSize);
  unsigned char *p = reinterpret_cast< unsigned char * >(dataBuf.data());
  if (hmapDataSize)
  {
    hmapData = reinterpret_cast< std::uint16_t * >(p);
    p = p + hmapDataSize;
  }
  if (ltexData32Size)
  {
    ltexData32 = reinterpret_cast< std::uint32_t * >(p);
    for (size_t i = 0; i < (ltexData32Size / sizeof(std::uint32_t)); i++)
      ltexData32[i] = 0U;
    p = p + ltexData32Size;
  }
  if (vnmlDataSize)
  {
    vnmlData = p;
    p = p + vnmlDataSize;
  }
  if (vclrData24Size)
  {
    vclrData24 = p;
    p = p + vclrData24Size;
  }
  if (ltexData16Size)
  {
    ltexData16 = reinterpret_cast< std::uint16_t * >(p);
    p = p + ltexData16Size;
  }
  if (txtSetDataSize)
  {
    txtSetData = p;
    for (size_t i = 0; i < txtSetDataSize; i++)
      txtSetData[i] = 0xFF;
  }
}

void LandscapeData::loadBTDFile(
    const unsigned char *btdFileData, size_t btdFileSize,
    unsigned int formatMask, unsigned char mipLevel)
{
  if (mipLevel > 4)
    errorMessage("LandscapeData: invalid mip level");
  cellResolution = 128 >> mipLevel;
  BTDFile btdFile(btdFileData, btdFileSize);
  if (btdFile.getCellMinX() > cellMinX)
    cellMinX = btdFile.getCellMinX();
  if (btdFile.getCellMinY() > cellMinY)
    cellMinY = btdFile.getCellMinY();
  if (btdFile.getCellMaxX() < cellMaxX)
    cellMaxX = btdFile.getCellMaxX();
  if (btdFile.getCellMaxY() < cellMaxY)
    cellMaxY = btdFile.getCellMaxY();
  zMin = btdFile.getMinHeight();
  zMax = btdFile.getMaxHeight();
  size_t  ltexCnt = 0;
  if (formatMask & 0x02)
    ltexCnt = btdFile.getLandTextureCount();
  ltexFormIDs.resize(ltexCnt);
  for (size_t i = 0; i < ltexCnt; i++)
    ltexFormIDs[i] = btdFile.getLandTexture(i);
  ltexEDIDs.resize(ltexCnt);
  ltexMaterials.resize(ltexCnt);
  for (size_t i = 0; i < ltexMaterials.size(); i++)
    ltexMaterials[i].texturePaths.setTexturePath(0, "");
  allocateDataBuf(formatMask, true);
  size_t  n = size_t(cellResolution);
  unsigned char m = 7 - mipLevel;
  std::vector< std::uint16_t >  tmpBuf(n * n);
  int     x0 = cellMinX;
  int     y0 = cellMaxY;
  int     x = cellMinX;
  int     y = cellMaxY;
  while (true)
  {
    size_t  w = size_t(cellMaxX + 1 - cellMinX);
    if (formatMask & 0x01)              // height map
    {
      std::uint16_t *bufp16 = tmpBuf.data();
      btdFile.getCellHeightMap(bufp16, x, y, mipLevel);
      for (size_t yy = 0; yy < n; yy++)
      {
        size_t  offs = size_t((cellMaxY - y) << m) | (~yy & (n - 1));
        offs = ((offs * w) + size_t(x - cellMinX)) << m;
        for (size_t xx = 0; xx < n; xx++, bufp16++, offs++)
          hmapData[offs] = *bufp16;
      }
    }
    if (formatMask & 0x02)              // land texture
    {
      std::uint16_t *bufp16 = tmpBuf.data();
      btdFile.getCellLandTexture(bufp16, x, y, mipLevel);
      for (size_t yy = 0; yy < n; yy++)
      {
        size_t  offs = size_t((cellMaxY - y) << m) | (~yy & (n - 1));
        offs = ((offs * w) + size_t(x - cellMinX)) << m;
        for (size_t xx = 0; xx < n; xx++, bufp16++, offs++)
          ltexData16[offs] = *bufp16;
      }
    }
    if (formatMask & 0x02)              // texture set
    {
      unsigned char   *bufp8 =
          reinterpret_cast< unsigned char * >(tmpBuf.data());
      btdFile.getCellTextureSet(bufp8, x, y);
      for (size_t yy = 0; yy < 2; yy++)
      {
        size_t  offs = size_t((cellMaxY - y) << 1) | (~yy & 1);
        offs = ((offs * w) + size_t(x - cellMinX)) << 5;
        for (size_t xx = 0; xx < 32; xx++, bufp8++, offs++)
        {
          unsigned char tmp = *bufp8;
          if ((xx & 8) && tmp != 0xFF)
            tmp = (unsigned char) ((ltexCnt + tmp) & 0xFF);
          txtSetData[offs] = tmp;
        }
      }
    }
    x++;
    if (x > cellMaxX || ((x - btdFile.getCellMinX()) & 7) == 0)
    {
      y--;
      if (y < cellMinY || ((y - btdFile.getCellMinY()) & 7) == 7)
      {
        if (x > cellMaxX)
        {
          if (y < cellMinY)
            break;
          x = cellMinX;
          y0 = y;
        }
        x0 = x;
        y = y0;
      }
      x = x0;
    }
  }
}

void LandscapeData::findESMLand(ESMFile& esmFile,
                                std::vector< unsigned long long >& landList,
                                unsigned int formID, int d, int x, int y)
{
  const ESMFile::ESMRecord  *r = (ESMFile::ESMRecord *) 0;
  for ( ; formID; formID = r->next)
  {
    r = esmFile.findRecord(formID);
    if (!r)
      break;
    if (*r == "GRUP")
    {
      if (!(r->children && r->formID >= 1 && r->formID <= 9))
        continue;
      if (r->formID != 2 && r->formID != 3 && r->formID != 7 && d < 8)
        findESMLand(esmFile, landList, r->children, d + 1, x, y);
    }
    else if (*r == "CELL")
    {
      ESMFile::ESMField f(esmFile, *r);
      while (f.next())
      {
        if (f == "XCLC" && f.size() >= 8)
        {
          x = f.readInt32();
          y = f.readInt32();
        }
      }
    }
    else if (*r == "LAND" &&
             x >= cellMinX && x <= cellMaxX && y >= cellMinY && y <= cellMaxY)
    {
      landList.push_back(((unsigned long long) (y + 32768) << 48)
                         | ((unsigned long long) (x + 32768) << 32)
                         | (unsigned long long) formID);
    }
  }
}

unsigned char LandscapeData::findTextureID(unsigned int formID)
{
  if (!formID)
    return 0xFF;
  for (size_t i = 0; i < ltexFormIDs.size(); i++)
  {
    if (ltexFormIDs[i] == formID)
      return (unsigned char) i;
  }
  if (ltexFormIDs.size() >= 255)
    errorMessage("LandscapeData: number of unique land textures > 255");
  ltexFormIDs.push_back(formID);
  return (unsigned char) (ltexFormIDs.size() - 1);
}

void LandscapeData::loadESMFile(ESMFile& esmFile,
                                unsigned int formatMask, unsigned int worldID,
                                unsigned int defTxtID, unsigned char mipLevel,
                                unsigned int parentWorldID)
{
  if (mipLevel != 2)
    errorMessage("LandscapeData: invalid mip level");
  std::vector< unsigned long long > landList;
  {
    const ESMFile::ESMRecord  *r = esmFile.findRecord(worldID);
    if (r && *r == "WRLD" && r->next)
    {
      r = esmFile.findRecord(r->next);
      if (r && *r == "GRUP" && r->formID == 1 && r->children)
        findESMLand(esmFile, landList, r->children);
    }
  }
  int     xMin = 32767;
  int     yMin = 32767;
  int     xMax = -32768;
  int     yMax = -32768;
  for (size_t i = 0; i < landList.size(); i++)
  {
    int     x = int((landList[i] >> 32) & 0xFFFFU) - 32768;
    int     y = int((landList[i] >> 48) & 0xFFFFU) - 32768;
    xMin = (x < xMin ? x : xMin);
    yMin = (y < yMin ? y : yMin);
    xMax = (x > xMax ? x : xMax);
    yMax = (y > yMax ? y : yMax);
  }
  if (!(xMax >= xMin && yMax >= yMin))
  {
    if (parentWorldID)
    {
      // if this worldspace has no landscape data, try the parent world
      loadESMFile(esmFile, formatMask, parentWorldID, defTxtID, mipLevel, 0U);
      return;
    }
    errorMessage("LandscapeData: "
                 "landscape data not found for world in ESM file");
  }
  cellMinX = xMin;
  cellMinY = yMin;
  cellMaxX = xMax;
  cellMaxY = yMax;
  allocateDataBuf(formatMask, false);
  std::map< unsigned int, unsigned int >  emptyCells;
  std::map< unsigned int, float > cellHeightOffsets;
  for (int y = cellMinY; y <= cellMaxY; y++)
  {
    for (int x = cellMinX; x <= cellMaxX; x++)
    {
      unsigned int  k =
          ((unsigned int) (y + 32768) << 16) | (unsigned int) (x + 32768);
      emptyCells.insert(std::pair< unsigned int, unsigned int >(k, formatMask));
      cellHeightOffsets.insert(std::pair< unsigned int, float >(k, landLevel));
    }
  }
  zMin = landLevel;
  zMax = landLevel;
  for (size_t i = 0; i < landList.size(); i++)
  {
    unsigned int  k = (unsigned int) ((landList[i] >> 32) & 0xFFFFFFFFU);
    std::map< unsigned int, unsigned int >::iterator  j = emptyCells.find(k);
    if (j == emptyCells.end())
      continue;
    const ESMFile::ESMRecord  *r =
        esmFile.findRecord((unsigned int) (landList[i] & 0xFFFFFFFFU));
    if (!(r && *r == "LAND"))
      continue;
    int     x = (int(k & 0xFFFFU) - (cellMinX + 32768)) << 5;
    int     y = ((cellMaxY + 32768 - int((k >> 16) & 0xFFFFU)) << 5) + 31;
    size_t  w = size_t(cellMaxX + 1 - cellMinX) << 5;
    size_t  dataOffs = size_t(y) * w + size_t(x);
    unsigned int  cellDataValidMask = 0;
    unsigned int  textureQuadrant = 0;
    unsigned int  textureLayer = 0;
    ESMFile::ESMField f(esmFile, *r);
    while (f.next())
    {
      if (f == "VHGT" && (formatMask & 0x01) && f.size() >= 1093)
      {
        // vertex heights
        float   zOffs = f.readFloat() * 8.0f;
        cellHeightOffsets[k] = zOffs;
        int     z = 0;
        std::uint16_t *p = hmapData + dataOffs;
        for (int yy = 0; yy < 32; yy++, p = p - (w + 32))
        {
          int     tmp = f.readUInt8Fast();
          z += (!(tmp & 0x80) ? tmp : (tmp - 256));
          *(p++) = (std::uint16_t) (z + 32768);
          float   zf = zOffs + float(z << 3);
          zMin = (zf < zMin ? zf : zMin);
          zMax = (zf > zMax ? zf : zMax);
          int     z0 = z;
          for (int xx = 1; xx < 32; xx++)
          {
            tmp = f.readUInt8Fast();
            z += (!(tmp & 0x80) ? tmp : (tmp - 256));
            *(p++) = (std::uint16_t) (z + 32768);
            zf = zOffs + float(z << 3);
            zMin = (zf < zMin ? zf : zMin);
            zMax = (zf > zMax ? zf : zMax);
          }
          z = z0;
          (void) f.readUInt8Fast();
        }
        cellDataValidMask |= 1U;
      }
      else if (f == "VNML" && (formatMask & 0x04) && f.size() >= 3267)
      {
        // vertex normals
        unsigned char *p = vnmlData + (dataOffs * 3);
        for (int yy = 0; yy < 32; yy++, p = p - ((w + 32) * 3))
        {
          for (int xx = 0; xx < 32; xx++, p = p + 3)
          {
            p[2] = (unsigned char) (f.readUInt8Fast() ^ 0x80);
            p[1] = (unsigned char) (f.readUInt8Fast() ^ 0x80);
            p[0] = (unsigned char) (f.readUInt8Fast() ^ 0x80);
          }
          f.setPosition(f.getPosition() + 3);
        }
        cellDataValidMask |= 4U;
      }
      else if (f == "VCLR" && (formatMask & 0x08) && f.size() >= 3267)
      {
        // vertex colors
        unsigned char *p = vclrData24 + (dataOffs * 3);
        for (int yy = 0; yy < 32; yy++, p = p - ((w + 32) * 3))
        {
          for (int xx = 0; xx < 32; xx++, p = p + 3)
          {
            p[2] = f.readUInt8Fast();
            p[1] = f.readUInt8Fast();
            p[0] = f.readUInt8Fast();
          }
          f.setPosition(f.getPosition() + 3);
        }
        cellDataValidMask |= 8U;
      }
      else if ((formatMask & 0x02) && f.size() >= 8)
      {
        // vertex textures
        if (f == "BTXT" || f == "ATXT")
        {
          unsigned char textureID = findTextureID(f.readUInt32Fast());
          textureQuadrant = f.readUInt16Fast() & 3U;
          textureLayer = 0;
          if (f == "ATXT")
            textureLayer = (f.readUInt16Fast() & 7U) + 1;
          size_t  yy = (size_t(y) >> 4) - ((textureQuadrant & 2U) >> 1);
          size_t  xx = (size_t(x) >> 4) + (textureQuadrant & 1U);
          txtSetData[(yy * w) + (xx << 4) + textureLayer] = textureID;
          cellDataValidMask |= 2U;
        }
        else if (f == "VTXT" && textureLayer)
        {
          while ((f.getPosition() + 8) <= f.size())
          {
            unsigned int  n = f.readUInt32Fast() & 0xFFFFU;
            int     a = int(f.readFloat() * 15.0f + 0.5f);
            a = (a >= 0 ? (a <= 15 ? a : 15) : 0);
            unsigned int  xx = n % 17U;
            unsigned int  yy = (n / 17U) % 17U;
            if ((xx | yy) & 16)
              continue;
            std::uint32_t *p = ltexData32 + dataOffs;
            p = p + (xx + ((textureQuadrant & 1U) << 4));
            p = p - ((yy + ((textureQuadrant & 2U) << 3)) * w);
            std::uint32_t m = 15U << ((textureLayer - 1U) << 2);
            *p = (*p & ~m) | (((std::uint32_t) a * 0x11111111U) & m);
          }
        }
      }
    }
    j->second = j->second & ~cellDataValidMask;
  }
  for (std::map< unsigned int, unsigned int >::iterator i = emptyCells.begin();
       i != emptyCells.end(); i++)
  {
    if (i->second == 0U)
      continue;
    // fill empty cells with default data
    int     x = int(i->first & 0xFFFFU) - (cellMinX + 32768);
    int     y = (cellMaxY + 32768) - int((i->first >> 16) & 0xFFFFU);
    size_t  w = size_t(cellMaxX + 1 - cellMinX) << 5;
    size_t  dataOffs = size_t((y << 5) + 31) * w + size_t(x << 5);
    for (int yy = 0; yy < 32; yy++, dataOffs = dataOffs - (w + 32))
    {
      for (int xx = 0; xx < 32; xx++, dataOffs++)
      {
        if (i->second & 0x01)
          hmapData[dataOffs] = 0x8000;
        if (i->second & 0x02)
          ltexData32[dataOffs] = 0U;
        if (i->second & 0x04)
        {
          vnmlData[dataOffs * 3] = 0xFF;
          vnmlData[dataOffs * 3 + 1] = 0x80;
          vnmlData[dataOffs * 3 + 2] = 0x80;
        }
        if (i->second & 0x08)
        {
          vclrData24[dataOffs * 3] = 0xFF;
          vclrData24[dataOffs * 3 + 1] = 0xFF;
          vclrData24[dataOffs * 3 + 2] = 0xFF;
        }
      }
    }
  }
  if (formatMask & 0x01)
  {
    // convert height map to normalized 16-bit unsigned integer format
    double  zScale = 0.0;
    if (zMax > zMin)
      zScale = 65535.0 / (double(zMax) - double(zMin));
    for (std::map< unsigned int, float >::iterator
             i = cellHeightOffsets.begin(); i != cellHeightOffsets.end(); i++)
    {
      int     x = int(i->first & 0xFFFFU) - (cellMinX + 32768);
      int     y = (cellMaxY + 32768) - int((i->first >> 16) & 0xFFFFU);
      size_t  w = size_t(cellMaxX + 1 - cellMinX) << 5;
      size_t  dataOffs = size_t((y << 5) + 31) * w + size_t(x << 5);
      double  zOffset = (double(i->second) - double(zMin)) * zScale + 0.5;
      for (int yy = 0; yy < 32; yy++, dataOffs = dataOffs - (w + 32))
      {
        for (int xx = 0; xx < 32; xx++, dataOffs++)
        {
          double  z = double((int(hmapData[dataOffs]) - 32768) << 3);
          int     tmp = int(z * zScale + zOffset);
          tmp = (tmp >= 0 ? (tmp <= 65535 ? tmp : 65535) : 0);
          hmapData[dataOffs] = (std::uint16_t) tmp;
        }
      }
    }
  }
  if (formatMask & 0x02)
  {
    if (defTxtID)
    {
      // replace missing textures with default texture ID
      unsigned char defaultTexture = findTextureID(defTxtID);
      size_t  n = (size_t(cellMaxX + 1 - cellMinX)
                   * size_t(cellMaxY + 1 - cellMinY)) << 6;
      for (size_t i = 0; i < n; i++)
      {
        if (txtSetData[i] == 0xFF && (i & 15) <= 8)
          txtSetData[i] = defaultTexture;
      }
    }
    ltexEDIDs.resize(ltexFormIDs.size());
    ltexMaterials.resize(ltexFormIDs.size());
    for (size_t i = 0; i < ltexMaterials.size(); i++)
      ltexMaterials[i].texturePaths.setTexturePath(0, "");
  }
}

void LandscapeData::loadTextureInfo(ESMFile& esmFile, const BA2File *ba2File,
                                    size_t n)
{
  const ESMFile::ESMRecord  *r = esmFile.findRecord(ltexFormIDs[n]);
  if (!r)
    return;
  std::string stringBuf;
  ESMFile::ESMField f(esmFile, *r);
  while (f.next())
  {
    if (f == "EDID" && ltexEDIDs[n].empty())
    {
      f.readString(ltexEDIDs[n]);
    }
    else if ((f == "BNAM" && *r == "LTEX") ||
             (f == "MNAM" && *r == "TXST"))
    {
      f.readPath(stringBuf, std::string::npos, "materials/", ".bgsm");
      ltexMaterials[n].texturePaths.setMaterialPath(stringBuf);
    }
    else if (f.size() >= 4 &&
             ltexMaterials[n].texturePaths.materialPath().empty() &&
             ltexMaterials[n].texturePaths[0].empty() &&
             ((f == "TNAM" && *r == "LTEX") ||
              (f == "LNAM" && *r == "GCVR")))
    {
      unsigned int  tmp = f.readUInt32Fast();
      unsigned int  savedFormID = ltexFormIDs[n];
      const ESMFile::ESMRecord  *r2 = esmFile.findRecord(tmp);
      if (tmp && r2 &&
          ((*r == "LTEX" && *r2 == "TXST") || (*r == "GCVR" && *r2 == "LTEX")))
      {
        ltexFormIDs[n] = tmp;
        loadTextureInfo(esmFile, ba2File, n);
        ltexFormIDs[n] = savedFormID;
      }
    }
    else if ((f.type & 0xF0FFFFFFU) == 0x30305854U && *r == "TXST")
    {
      // f == "TX0*"
      size_t  i = (f.type >> 24) & 15U;
      unsigned int  v = esmFile.getESMVersion() & ~0x3FU;
      if (i < 2 || i == 3 ||                    // diffuse, normal, or glow
          (i == 7 && v == 0x80U) ||             // FO4 specular map
          ((i & 14) == 8 && v == 0xC0U))        // FO76 reflectance or lighting
      {
        f.readPath(stringBuf, std::string::npos, "textures/", ".dds");
        if (!(~i & 3))
          i--;                  // 3 -> 2, 7 -> 6
        ltexMaterials[n].texturePaths.setTexturePath(i, stringBuf.c_str());
      }
    }
    else if (f == "ICON" && *r == "LTEX")       // TES4
    {
      f.readPath(stringBuf, std::string::npos, "textures/landscape/", ".dds");
      ltexMaterials[n].texturePaths.setTexturePath(0, stringBuf.c_str());
      if (ba2File && !stringBuf.empty())
      {
        stringBuf.insert(stringBuf.length() - 4, "_n");
        if (ba2File->getFileSize(stringBuf) > 0L)
          ltexMaterials[n].texturePaths.setTexturePath(1, stringBuf.c_str());
      }
    }
  }
  if (ba2File && !ltexMaterials[n].texturePaths.materialPath().empty())
  {
    try
    {
      stringBuf = ltexMaterials[n].texturePaths.materialPath();
      BGSMFile  bgsmFile(*ba2File, stringBuf);
      bgsmFile.texturePaths.setMaterialPath(stringBuf);
      ltexMaterials[n] = bgsmFile;
    }
    catch (FO76UtilsError&)
    {
    }
  }
}

unsigned int LandscapeData::loadWorldInfo(ESMFile& esmFile,
                                          unsigned int worldID)
{
  const ESMFile::ESMRecord  *r = esmFile.findRecord(worldID);
  if (!(r && *r == "WRLD"))
    errorMessage("LandscapeData: world not found in ESM file");
  unsigned int  parentID = 0U;
  ESMFile::ESMField f(esmFile, *r);
  while (f.next())
  {
    if (f == "DNAM" && f.size() >= 8)
    {
      landLevel = f.readFloat();
      waterLevel = f.readFloat();
    }
    else if (f == "NAM2" && f.size() >= 4)
    {
      waterFormID = f.readUInt32Fast();
    }
    else if (f == "WNAM" && f.size() >= 4)
    {
      unsigned int  tmp = f.readUInt32Fast();
      if (tmp && tmp < worldID)
      {
        parentID = tmp;
        loadWorldInfo(esmFile, parentID);
      }
    }
  }
  return parentID;
}

LandscapeData::LandscapeData(
    ESMFile *esmFile, const char *btdFileName, const BA2File *ba2File,
    unsigned int formatMask, unsigned int worldID, unsigned int defTxtID,
    int mipLevel, int xMin, int yMin, int xMax, int yMax)
  : cellMinX(xMin),
    cellMinY(yMin),
    cellMaxX(xMax),
    cellMaxY(yMax),
    cellResolution(32),
    hmapData((std::uint16_t *) 0),
    ltexData32((std::uint32_t *) 0),
    vnmlData((unsigned char *) 0),
    vclrData24((unsigned char *) 0),
    ltexData16((std::uint16_t *) 0),
    txtSetData((unsigned char *) 0),
    zMin(1.0e9f),
    zMax(-1.0e9f),
    waterLevel(0.0f),
    landLevel(1024.0f),
    waterFormID(0x00000018U)
{
  unsigned char l = (unsigned char) mipLevel;
  if (esmFile)
  {
    if (esmFile->getESMVersion() >= 0xC0U && !btdFileName)
      btdFileName = "";
    else if (esmFile->getESMVersion() < 0xC0U && btdFileName)
      btdFileName = (char *) 0;
  }
  if (!worldID)
    worldID = (btdFileName ? 0x0025DA15U : 0x0000003CU);
  unsigned int  parentID = 0U;
  if (esmFile)
  {
    waterLevel = 0.0f;          // defaults for Oblivion
    landLevel = -8192.0f;
    parentID = loadWorldInfo(*esmFile, worldID);
  }
  if (btdFileName)
  {
    std::string btdFullName;
    const char  *s = btdFileName;
    size_t  n = std::strlen(s);
    if (!(n > 4 && (FileBuffer::readUInt32Fast(s + (n - 4)) & 0xDFDFDFFFU)
                   == 0x4454422EU))     // ".BTD"
    {
      if (!ba2File)
      {
        btdFullName = btdFileName;
        if (!btdFullName.empty())
        {
          if (btdFullName.back() != '/'
#if defined(_WIN32) || defined(_WIN64)
              && btdFullName.back() != '\\' && btdFullName.back() != ':'
#endif
              )
          {
            btdFullName += '/';
          }
        }
        btdFullName += "Terrain/";
      }
      else
      {
        btdFullName = "terrain/";
      }
      bool    nameFound = false;
      if (esmFile)
      {
        const ESMFile::ESMRecord  *r = esmFile->findRecord(worldID);
        if (r && *r == "WRLD")
        {
          ESMFile::ESMField f(*esmFile, *r);
          while (f.next())
          {
            if (f == "EDID" && f.size() > 0)
            {
              for (size_t i = f.size(); i > 0; i--)
              {
                char    c = char(f.readUInt8Fast());
                if (!c)
                  break;
                if (!((unsigned char) c >= 0x20 && (unsigned char) c < 0x7F))
                  c = '_';
                if (ba2File && c >= 'A' && c <= 'Z')
                  c = c + ('a' - 'A');
                btdFullName += c;
              }
              nameFound = true;
              break;
            }
          }
        }
      }
      if (!nameFound)
        errorMessage("LandscapeData: no BTD file name");
      else
        btdFullName += ".btd";
      s = btdFullName.c_str();
    }
    if (ba2File)
    {
      std::vector< unsigned char >  btdBuf;
      const unsigned char *btdFileData = (unsigned char *) 0;
      size_t  btdFileSize =
          ba2File->extractFile(btdFileData, btdBuf, std::string(s));
      loadBTDFile(btdFileData, btdFileSize, formatMask & 0x03, l);
    }
    else
    {
      FileBuffer  btdBuf(s);
      loadBTDFile(btdBuf.data(), btdBuf.size(), formatMask & 0x03, l);
    }
  }
  else if (esmFile)
  {
    loadESMFile(*esmFile, formatMask & 0x0F, worldID, defTxtID, l, parentID);
  }
  else
  {
    errorMessage("LandscapeData: no input file");
  }
  if (esmFile)
  {
    for (size_t i = 0; i < ltexFormIDs.size(); i++)
      loadTextureInfo(*esmFile, ba2File, i);
  }
}

LandscapeData::~LandscapeData()
{
}

