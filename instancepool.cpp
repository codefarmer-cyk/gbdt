#include<sstream>
#include<fstream>
#include<time.h>
#include"unity.h"
#include"DecisionTree.h"
#include"mempool.h" 
#include"threadpool.h"
#include"instancepool.h"

namespace gbdt 
{
	Instance::Instance(){}
	Instance::~Instance(){}
	std::string Instance::ToString()
	{
		std::ostringstream oss;
		for(uint32 i=0;i<X.size();i++)
		{
			if(i != 0)
			{
				oss<<",";
			}
			oss<<X[i];
		}
//		oss<<","<<ys;
		return oss.str();
	}
	std::string Instance::DebugStr()
	{
		std::ostringstream oss;
		oss << "X:";
		for(uint32 i=0;i<X.size();i++)
		{
			oss<<X[i]<<" ";
		}
		oss<<"y:"<<y<<" ys:"<<ys<<" weight:"<<weight<<" index:"<<index;
		return oss.str();
	}
	void Instance::print()
	{
		std::string str = DebugStr();
		printf("%s\n",str.c_str());	
	}


	InstancePool::InstancePool(GbdtConf *pconfig):m_pconfig(pconfig)
	{}
	InstancePool::~InstancePool()
	{}
	int InstancePool::GetSubSamplesPtr(
			Instance ** & ppRetInstances, 
			int & RetInstanceNum
			)
	{
		return GetSubSamplesPtr(m_pconfig->SubSampleRate , ppRetInstances, RetInstanceNum);
	}
	int InstancePool::GetSubSamplesPtr(
			FloatT SubSampleRate, 
			Instance ** & ppRetInstances, 
			int & RetInstanceNum
			)
	{
		srand(clock());
		if(m_Instances.size()<= 0 )
		{
			Comm::LogErr("InstancePool::GetSubSamplesPtr No Instance!");
			return -1;
		}
		int instanceNum = m_Instances.size();
		int sampleNum = (int)(instanceNum * SubSampleRate);
		if(sampleNum < 1) sampleNum = 1;
		int * tmp = (int * )malloc((instanceNum + 7) * sizeof(int));
		if(!tmp)
		{
			Comm::LogErr("InstancePool::GetSubSamplesPtr tmp is NULL");
			return -1;
		}
		ppRetInstances = (Instance **)malloc((sampleNum + 7) * sizeof(Instance *));
		if(!ppRetInstances)
		{
			Comm::LogErr("InstancePool::GetSubSamplesPtr ppRetInstances is NULL");
			tmp = Comm::Free(tmp);
			return -1;
		}
		int cnt = 0;
		for( int i = 0; i < instanceNum; i++)
		{
			tmp[i] = i;
		}
		srand(time(NULL));
		int last = instanceNum;
		for(int i = 0; i < sampleNum; i++)
		{
			int select = rand() % last;
			int fid = tmp[select];
			ppRetInstances[cnt] = & m_Instances[fid];
			cnt++;
			last--;
			tmp[select] = tmp[last];
		}
		RetInstanceNum = cnt;
		tmp = Comm::Free(tmp);
		return 0;
	}
	int InstancePool::GetSubSamplesPtr(
			FloatT SubSampleRate, 
			Instance ** ppInstances,
			int instanceNum,
			Instance ** & ppRetInstances, 
			int & RetInstanceNum
			)
	{
		srand(clock());
		if(instanceNum <= 0 )
		{
			Comm::LogErr("InstancePool::GetSubSamplesPtr No Instance!");
			return -1;
		}
		int sampleNum = (int)(instanceNum * SubSampleRate);
		if(sampleNum < 1) sampleNum = 1;
		int * tmp = (int * )malloc((instanceNum + 7) * sizeof(int));
		if(!tmp)
		{
			Comm::LogErr("InstancePool::GetSubSamplesPtr tmp is NULL");
			return -1;
		}
		ppRetInstances = (Instance **)malloc((sampleNum + 7) * sizeof(Instance *));
		if(!ppRetInstances)
		{
			Comm::LogErr("InstancePool::GetSubSamplesPtr ppRetInstances is NULL");
			tmp = Comm::Free(tmp);
			return -1;
		}
		int cnt = 0;
		for( int i = 0; i < instanceNum; i++)
		{
			tmp[i] = i;
		}
		srand(time(NULL));
		int last = instanceNum;
		for(int i = 0; i < sampleNum; i++)
		{
			int select = rand() % last;
			int fid = tmp[select];
			ppRetInstances[cnt] =  ppInstances[fid];
			cnt++;
			last--;
			tmp[select] = tmp[last];
		}
		RetInstanceNum = cnt;
		tmp = Comm::Free(tmp);
		return 0;
	}

	int InstancePool::GetSubFeatureIDs(std::vector<uint32> & SubFeatures)
	{
		return GetSubFeatureIDs(m_pconfig->SubFeatureRate, SubFeatures);
	}
	int InstancePool::GetSubFeatureIDs(
			FloatT SubFeatureRate, 
			std::vector<uint32> & SubFeatures
			)
	{
		int FeatureNum = m_pconfig->FeatureNum;
		int SubFeatureNum = (int)(FeatureNum * SubFeatureRate);
		if(SubFeatureNum < 1)SubFeatureNum = 1;
		int * tmp = (int * )malloc((FeatureNum + 7)*sizeof(int));
		if(!tmp)
		{
			Comm::LogErr("InstancePool::GetSubFeatureIDs tmp is NULL");
			return -1;
		}
		SubFeatures.clear();
		for(int i=0;i<FeatureNum;i++)
		{
			tmp[i]=i;
		}
		int last = FeatureNum;
		srand(time(NULL));
		for(int i=0;i<SubFeatureNum;i++)
		{
			int select = rand() % last;
			int fid = tmp[select];
			SubFeatures.push_back(fid);
			last--;
			tmp[select] = tmp[last];
		}
		return 0;
	}
	int InstancePool::Input()
	{
		return Input(m_pconfig->InputDataFilePath);
	}
	int InstancePool::Input(const std::string & InputDataFilePath)
	{
		std::ifstream fpinput(InputDataFilePath.c_str());
		if(!fpinput)
		{
			Comm::LogErr("InstancePool::Input can not open input file %s",InputDataFilePath.c_str());
			return -1;
		}
		std::string line;
		int cnt = 0;
		while( getline(fpinput,line)!=NULL)
		{
			cnt++;
			if(line.find("n_feature") != line.npos)
			{
				std::vector<std::string>col;
				Comm::stringHelper::split(line.c_str(),", \t\r\n",col);
				if(col.size() < 2)
				{
					Comm::LogErr("InstancePool::Input %s illegal!",line.c_str());
					continue;
				}
				if("n_feature" != col[0])
				{
					Comm::LogErr("InstancePool::Input %s illegal!",line.c_str());
					continue;
				}
				m_pconfig->FeatureNum = atoi(col[1].c_str());
				printf("InstancePool::m_pconfig->FeatureNum = %d\n",m_pconfig->FeatureNum);
				Comm::LogInfo("InstancePool::m_pconfig->FeatureNum = %d",m_pconfig->FeatureNum);	
				continue;
			}
			
			bool flag = false;
			for(uint32 i=0;i<line.size();i++)
			{
				if(!Comm::stringHelper::isInSpset(line[i], "1234567890,.eE- \t\r\n"))
				{
					printf("the = %d\n",line[i]);
					printf("%s\n",line.c_str());
					Comm::LogInfo("%s",line.c_str());
					flag = true;
					break;
				}
			}
			if(flag)
			{
				continue;
			}
			std::vector<std::string>col;
			Comm::stringHelper::split(line.c_str(),", \t\r\n",col);
			if(col.size() < m_pconfig->FeatureNum + 1)
			{
				printf("InstancePool::m_pconfig->FeatureNum(%d) != now FeatureNum(%d) %s\n",m_pconfig->FeatureNum,col.size(),line.c_str());
				Comm::LogInfo("InstancePool::m_pconfig->FeatureNum(%d) != now FeatureNum(%d) %s",m_pconfig->FeatureNum,col.size(),line.c_str());
				continue;
			}
			Instance instance;
			int i;
			FloatT weight = atof(col[0].c_str());
			FloatT y = atof(col[1].c_str());
			for(i=2;i<m_pconfig->FeatureNum + 2;i++)
			{
				FloatT x = atof(col[i].c_str());
				instance.X.push_back(x);
			}
			//instance.X_BucketIndex.resize(instance.X.size());
			instance.y = y;
			instance.ys = y;
			instance.weight = weight;
			instance.index = m_Instances.size();
			m_Instances.push_back(instance);
		}
		//MakeBucket();
		printf("InstancePool::Input input lines num = %d instances size = %d\n", cnt, m_Instances.size());
		Comm::LogInfo("InstancePool::Input input lines num = %d instances size = %d", cnt, m_Instances.size());

		return 0;
	}


	int InstancePool::ProcessBucket(int FeatureId, std::vector<FloatT> & vecFeature)
	{
		printf("FeatureId %d\n", FeatureId);
		std::vector<FloatT> vecFeature_tmp;
		vecFeature_tmp.resize(m_Instances.size());
		for(int i = 0; i < m_Instances.size(); i++)
		{
			vecFeature_tmp[i] = m_Instances[i].X[FeatureId];
		}
		std::sort(vecFeature_tmp.begin(), vecFeature_tmp.end());
		int len = std::unique(vecFeature_tmp.begin(), vecFeature_tmp.end()) - vecFeature_tmp.begin();
		//vecFeature.erase(iter, vecFeature.end());
		for(int i =0; i < len; i++)
		{
			vecFeature.push_back(vecFeature_tmp[i]);
		}
		for(int i = 0; i < m_Instances.size(); i++)
		{
			FloatT val = m_Instances[i].X[FeatureId];
			int bucketIndex = std::lower_bound(vecFeature.begin(), vecFeature.end(), val) - vecFeature.begin();
			m_Instances[i].X_BucketIndex[FeatureId] = bucketIndex;
		}
		return vecFeature.size();
	}


	void InstancePool::MakeBucket()
	{
		m_FeatureBucketMap.resize(m_pconfig->FeatureNum);
		#pragma omp parallel for
		for(int i = 0; i< m_Instances.size(); i++)
		{
			m_Instances[i].X_BucketIndex.resize(m_Instances[i].X.size());
		}
		#pragma omp parallel for
		for(int i = 0; i< m_pconfig->FeatureNum; i++)
		{
			int FeatureBucketSize = ProcessBucket(i, m_FeatureBucketMap[i]);
			//printf("FeatureBucketSize = %d \n", FeatureBucketSize);
		}

	}

	Instance & InstancePool::GetInstance(int index)
	{
		return m_Instances[index];
	}
	Instance & InstancePool::operator[](int index)
	{
		return GetInstance(index);
	}
	
	void InstancePool::AddInstance(const Instance & instance)
	{
		m_Instances.push_back(instance);
		//m_Instances[m_Instances.size()-1].weight = weight;
		m_Instances[m_Instances.size()-1].index = m_Instances.size()-1;
	}

	int InstancePool::Size()const
	{
		return m_Instances.size();
	}

	void InstancePool::print()
	{
		for(uint32 i=0;i<m_Instances.size();i++)
		{
			m_Instances[i].print();
		}
	}


}
