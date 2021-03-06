/**
 * @file classifier_wrap.h
 * @brief file contains wrap of existing classifiers from OpenCV library and LibSVM and LibLinear
 * @author Tran Tuan Hiep
 * @version 1.0
 * @date 2014-09-14
 */

#ifndef NOCRLIB_DESTREE_H 
#define NOCRLIB_DESTREE_H 

#include "swt.h"
#include "component.h"
#include "drawer.h"
#include "feature_traits.h"
#include "exception.h"
#include "train_data.h"
#include "assert.h"

#include <libsvm/svm.h>

#include <pugi/pugixml.hpp>


#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <memory>
#include <array>



#include <opencv2/highgui/highgui.hpp>
#include <opencv2/ml/ml.hpp>
#include <opencv2/objdetect/objdetect.hpp> 



/// @cond
template <feature F> struct LoadTrainData 
{ 
    static void load( const std::string &file_name, cv::Mat &train_data, cv::Mat &labels ) 
    { 
        int length = FeatureTraits<F>::features_length;
        TrainDataLoader train_data_loader( length ); 
        train_data_loader.prepareDataForTraining( file_name, train_data, labels );
    }
};

/// @endcond

template <template <feature>  class C, feature F>
std::shared_ptr< C<F> > create()
{
    return std::shared_ptr< C<F> >(new C<F>());
}

template <template <feature>  class C, feature F, typename... ARGS>
std::shared_ptr< C<F> > create(ARGS &&... args)
{
    return std::shared_ptr< C<F> >(new C<F>(std::forward<ARGS>(args)...));
}

/**
 * @brief wrap of opencv desion tree class
 *
 * @tparam F enum representing composite, to be used for extraction 
 */
template < feature F >
class DesionTree 
{
    public:
        DesionTree() { }
        ~DesionTree() { }
        
        /**
         * @brief train classifier
         *
         * @param data_file path to training data
         * @param param parameters for training
         *
         * For further details see the programming documentation.
         */
        void train ( const std::string &data_file,
              const CvDTreeParams &param = CvDTreeParams() )
        {
            cv::Mat train_data, labels;
            LoadTrainData<F>::load( data_file, train_data, labels );
            decision_tree_.train( train_data, CV_ROW_SAMPLE , labels, cv::Mat(), cv::Mat(), 
                    cv::Mat(), cv::Mat(), param );
        }
        
        /**
         * @brief save desion tree configuration to conf_file
         *
         * @param conf_file path to the configuration
         */
        void saveConfiguration( const std::string &conf_file )
        {
            decision_tree_.save( conf_file.data() );
        }
        
        /**
         * @brief loads configuration from \p conf_file
         *
         * @param conf_file path to the configuration
         * @throw FileNotFoundException when file \p conf_file doesn't exist
         */
        void loadConfiguration( const std::string &conf_file )
        {
            if ( fileExists(conf_file) )
            {
                decision_tree_.load( conf_file.data() );
                return;
            }
            throw FileNotFoundException(conf_file + ", desion tree configuration not found");
        }

        /**
         * @brief classifies the sample to label
         *
         * @param sample feature vector for classification
         *
         * @return label of class, sample is classified to 
         */
        float predict ( const cv::Mat &sample ) const 
        {
            return decision_tree_.predict( sample )->value;
        }

    private:
        CvDTree decision_tree_;
    
};




/**
 * @brief wrap of opencv boosting class
 *
 * @tparam F enum representing composite, to be used for extraction 
 */
template < feature F >
class Boost
{
    public:
        /**
         * @brief train boosting classificators
         *
         * @param data_file path to data with the train data
         * @param params parameters for learning of boosting algorithm
         *
         * See OpenCV implementation for detailed description of \p params.
         */
        void train( const std::string &data_file, const CvBoostParams &params )
        {
            cv::Mat train_data, labels;
            LoadTrainData<F>::load( data_file, train_data, labels );

            boost_.train( train_data, CV_ROW_SAMPLE, labels, cv::Mat(), cv::Mat(), cv::Mat(), cv::Mat(), params );
        }


        /**
         * @brief loads configuration from \p conf_file
         *
         * @param conf_file path to the configuration
         */
        void saveConfiguration( const std::string &conf_file )
        {
            boost_.save( conf_file.data() );
        }
        
        /**
         * @brief loads configuration from \p conf_file
         *
         * @param conf_file path to the configuration
         * @throw FileNotFoundException when file \p conf_file doesn't exist
         */
        void loadConfiguration( const std::string &conf_file )
        {
            if ( fileExists(conf_file) )
            {
                boost_.load(conf_file.data());
                return;
            }
            throw FileNotFoundException(conf_file + ", boosting configuration not found");
        }

        /**
         * @brief predicts labeled for class if flag to return sum is 
         * set to false else returns only weighted sum of decision functions
         *
         * @param sample features to be classified
         *
         * @return predicted label or weighted sum of decision functions
         */
        float predict ( const cv::Mat &sample ) const 
        {
            return boost_.predict( sample, cv::Mat(), cv::Range::all(), false, sum_ );
        }

        float predict (const std::vector<float> & sample) const 
        {
            cv::Mat tmp(1, sample.size(), CV_32FC1);

            for (std::size_t i = 0; i < sample.size(); ++i)
            {
                tmp.at<float>(0, i) = sample[i];
            }
             
            return boost_.predict( tmp, cv::Mat(), cv::Range::all(), false, sum_ );
        }

        /**
         * @brief set flag to return sum or labeled function
         *
         * @param sum if true returns sum, else returns labeled class
         */
        void setReturningSum( bool sum )
        {
            sum_ = sum;
        }
    private:
        CvBoost boost_;
        bool sum_;
}; 

/// @cond
struct FeatureScaler;

class LibSVMTrainBridge
{
    public:
        LibSVMTrainBridge() 
            : problem_(nullptr)
        {
        }

        ~LibSVMTrainBridge()
        {
            if (problem_)
            {
                delete[] problem_->y;
                for ( int i = 0; i < problem_->l; ++i )
                {
                    delete[] problem_->x[i];
                }
                 
                delete[] problem_->x;

                delete problem_;
            }
        }

        // kdyztak dopsat copy atd..
        
        svm_model* train( const cv::Mat &train_data, const cv::Mat &labels, svm_parameter *param );
        svm_node* constructSample( const std::vector<float> &output ) const;

        void constructSample( const std::vector<float> &output, svm_node * nodes ) const;

        void save(const std::string & file_name, const svm_model * model);
        void save(const std::string & file_name, const svm_model * model, 
                const std::vector<FeatureScaler> & scalers);

        svm_model * load(const std::string & file_name);
        svm_model * load(const std::string & file_name, std::vector<FeatureScaler> & scalers);

        void destroy_svm_model(svm_model ** model);
    private:
        void constructProblem( const cv::Mat &train_data, const cv::Mat &labels );
        void saveSvmModel(pugi::xml_node & svm_node, const svm_model * model);
        svm_model * loadSvmModel(const pugi::xml_node & svm_node);

        template <typename T> 
        std::string format(const T * ptr, std::size_t length, char delim = ' ')
        {
            std::stringstream ss;
            for (std::size_t i = 0; i < length - 1; ++i)
            {
                ss << ptr[i] << delim;
            }
            ss << ptr[length - 1];

            return ss.str();
        }

        template <typename T>
        void parse(T ** ptr, std::size_t length, const char * data)
        {
            std::stringstream ss(data);
            T * tmp = new T[length];
            for (int i = 0; i < length; ++i)
            {
                ss >> tmp[i];
            }

            *ptr = tmp;
        }

        svm_problem * problem_;
};
/// @endcond

/**
 * @brief wrap of SVM implementation from LibSVM
 *
 * @tparam F enum representing composite, to be used for extraction 
 *
 * This class integrates LibSVM implementation of SVM with C++. These SVM 
 * supports probability outputs.
 */
template <feature F>
class LibSVM
{
    public:
        LibSVM() 
        {
            svm_ = nullptr;
        }

        ~LibSVM() 
        {
            if ( svm_ != nullptr )
            {
                svm_free_and_destroy_model(&svm_);
            }
        }


        /**
         * @brief returns number of classes
         *
         * @return number of classes
         */
        int getNumberOfClasses() const 
        {
            return number_of_classes_; 
        }

        /**
         * @brief train data without any scaling
         *
         * @param data_file path to file
         * @param param parameter for svm training
         *
         * See the libsvm documentation for more detail on
         * parameters for training svm.
         */
        void train( const std::string &data_file, svm_parameter *param )
        {
            cv::Mat train_data, labels;
            LoadTrainData<F>::load( data_file, train_data, labels );
            svm_ = bridge_.train( train_data, labels, param );
            number_of_classes_ = svm_get_nr_class( svm_ );
        }

        /**
         * @brief loads configuration from \p conf_file
         *
         * @param conf_file path to the configuration
         */
        void saveConfiguration( const std::string &conf_file )
        {
            int result = svm_save_model( conf_file.c_str(), svm_ );
            if (result != 0)
            {
                throw ActionError( "saving to " + conf_file );
            }
        }

        /**
         * @brief loads configuration from \p conf_file
         *
         * @param conf_file path to the configuration
         * @throw FileNotFoundException when file \p conf_file doesn't exist
         */
        void loadConfiguration( const std::string &conf_file )
        {
            svm_ = svm_load_model( conf_file.c_str() );
            //TODO
            if ( svm_ == nullptr )
            {
                throw FileNotFoundException(conf_file + ", libsvm configuration not found");
            }
            number_of_classes_ = svm_get_nr_class( svm_ );
        }


        /**
         * @brief predicts class for feature vector sample
         *
         * @param sample vector of features
         *
         * @return label of predicted class
         */
        float predict(const std::vector<float> &data ) 
        {
            NOCR_ASSERT( svm_ != nullptr , "no configuration loaded yet" );

            bridge_.constructSample( data, &nodes_[0]);
            float out = svm_predict( svm_ , &nodes_[0]);
            return out;
        }

        /**
         * @brief predicts class for feature vector sample
         *
         * @param sample vector of features
         * @param probabilities probability outputs from classification 
         * will be stored in this vector
         *
         * @return label of predicted class
         *
         * If SVM isn't trained for probability outputs, exception will be thrown.
         */
        double predictProbabilities(const std::vector<float> &data, 
                                    std::vector<double> &probabilities ) 
        {
            NOCR_ASSERT( svm_ != nullptr , "no configuration loaded yet" );

            bridge_.constructSample( data, &nodes_[0] );
            probabilities.resize( number_of_classes_);
            double out = svm_predict_probability( svm_ , &nodes_[0], 
                                                probabilities.data() ); 
            return out;
        }

    private:
        LibSVMTrainBridge bridge_;
        svm_model *svm_;
        int number_of_classes_;
        std::array<svm_node, FeatureTraits<F>::features_length + 1> nodes_;
};

// /**
//  * @brief wrap of SVM implementation from LibSVM
//  *
//  * This class integrates LibSVM implementation of SVM with C++. These SVM 
//  * supports probability outputs.
//  */
// class SVM
// {
//     public:
//         SVM(int length) 
//             : length_(length)
//         {
//             svm_ = nullptr;
//         }
//
//         ~SVM() 
//         {
//             if ( svm_ != nullptr )
//             {
//                 svm_free_and_destroy_model(&svm_);
//             }
//         }
//
//
//         /**
//          * @brief returns number of classes
//          *
//          * @return number of classes
//          */
//         int getNumberOfClasses() const 
//         {
//             return number_of_classes_; 
//         }
//
//         /**
//          * @brief train data without any scaling
//          *
//          * @param data_file path to file
//          * @param param parameter for svm training
//          *
//          * See the libsvm documentation for more detail on
//          * parameters for training svm.
//          */
//         void train( const std::string &data_file, svm_parameter *param );
//
//         /**
//          * @brief loads configuration from \p conf_file
//          *
//          * @param conf_file path to the configuration
//          */
//         void saveConfiguration( const std::string &conf_file );
//
//         /**
//          * @brief loads configuration from \p conf_file
//          *
//          * @param conf_file path to the configuration
//          * @throw FileNotFoundException when file \p conf_file doesn't exist
//          */
//         void loadConfiguration( const std::string &conf_file );
//
//         /**
//          * @brief predicts class for feature vector sample
//          *
//          * @param sample vector of features
//          *
//          * @return label of predicted class
//          */
//         float predict(const std::vector<float> &data ) const;
//
//         /**
//          * @brief predicts class for feature vector sample
//          *
//          * @param sample vector of features
//          * @param probabilities probability outputs from classification 
//          * will be stored in this vector
//          *
//          * @return label of predicted class
//          *
//          * If SVM isn't trained for probability outputs, exception will be thrown.
//          */
//         double predictProbabilities(const std::vector<float> &data, 
//                                     std::vector<double> &probabilities ) const;
//
//     private:
//         LibSVMTrainBridge bridge_;
//         svm_model *svm_;
//         int length_;
//         int number_of_classes_;
// };


/// @cond
// class LibLINEARTrainBridge
// {
//     public:
//         model* trainModel( const cv::Mat &train_data,
//                 const cv::Mat &labels, parameter *param );
//
//         feature_node* constructSample
//             ( const std::vector<float> &output ) const;
//     private:
//         problem constructProblem
//             ( const cv::Mat &train_data, const cv::Mat &labels ) const;
// };
/// @endcond

/**
 * @brief wrap of svm with linear kernel from library LibLinear
 *
 * @tparam F enum representing specific descriptor
 */
// template <feature F>
// class LibLINEAR
// {
//     public:
//         LibLINEAR() = default;
//         ~LibLINEAR() 
//         {
//             free_and_destroy_model( &model_ );
//         }
//
//         int getNumberOfClasses() const 
//         {
//             return number_of_classes_; 
//         }
//
//         #<{(|*
//          * @brief train data without any scaling
//          *
//          * @param data_file path to file
//          * @param param parameter for svm training
//          *
//          * See the OpenCV documentation for more detail on
//          * parameters for training opencv.
//          |)}>#
//         void train( const std::string &data_file, parameter *param )
//         {
//             cv::Mat train_data, labels;
//             LoadTrainData<F>::load( data_file, train_data, labels );
//             
//             model_ = bridge_.trainModel( train_data, labels, param );
//             number_of_classes_ = get_nr_class( model_ );
//         }
//
//         #<{(|*
//          * @brief loads configuration from \p conf_file
//          *
//          * @param conf_file path to the configuration
//          |)}>#
//         void saveConfiguration( const std::string &conf_file )
//         {
//             int result = save_model( conf_file.c_str(), model_ );
//             if (result != 0)
//             {
//                 throw ActionError( "saving to " + conf_file );
//             }
//         }
//
//         #<{(|*
//          * @brief loads configuration from \p conf_file
//          *
//          * @param conf_file path to the configuration
//          * @throw FileNotFoundException when file \p conf_file doesn't exist
//          |)}>#
//         void loadConfiguration( const std::string &conf_file )
//         {
//             model_ = load_model( conf_file.c_str() );
//             //TODO
//             if ( model_ == nullptr )
//             {
//                 throw FileNotFoundException(conf_file + ", svm configuration not found");
//             }
//             number_of_classes_ = get_nr_class( model_ );
//         }
//
//         #<{(|*
//          * @brief predicts class for feature vector sample
//          *
//          * @param sample vector of features
//          *
//          * @return label of predicted class
//          |)}>#
//         float predictSample(const std::vector<float> &data ) const
//         {
//             NOCR_ASSERT( model_ != nullptr, "no configuration loaded" );
//
//             feature_node *nodes = bridge_.constructSample( data );
//             float out = predict( model_ , nodes );
//             delete[] nodes;
//             return out;
//         }
//
//         #<{(|*
//          * @brief predicts class for feature vector sample
//          *
//          * @param sample vector of features
//          * @param probabilities probability outputs from classification 
//          * will be stored in this vector
//          *
//          * @return label of predicted class
//          *
//          * If SVM isn't trained for probability outputs, exception will be thrown.
//          |)}>#
//         double predictProbabilities( const std::vector<float> &data, 
//                 std::vector<double> &probabilities ) const  
//         {
//             NOCR_ASSERT( model_ != nullptr, "no configuration loaded" );
//
//             feature_node *nodes = bridge_.constructSample( data );
//             probabilities.resize( number_of_classes_ );
//             double out = predict_probability( model_ , nodes, probabilities.data() ); 
//             delete[] nodes;
//             return out;
//         }
//
//     private:
//         LibLINEARTrainBridge bridge_;
//         model *model_; 
//         int number_of_classes_;
// };


/**
 * @brief class scales value to interval [0,1]
 */
struct FeatureScaler
{
    FeatureScaler() 
        : min_(0), interval_length_(1), scaled_min_(0), scaled_interval_length_(1)
    {
    }
    
    FeatureScaler( float min, float max, float scaled_min = -1, float scaled_max = 1 );
    float min_, interval_length_;
    float scaled_min_, scaled_interval_length_;

    float scale(float feature) const;
};

class DataScaling
{
    public:
        DataScaling() = default;
        DataScaling( const std::vector<FeatureScaler> &scalers )
            : scalers_(scalers) { }

        std::vector<float> scale( const std::vector<float> &descriptor ) const;

        void setUp( const cv::Mat &train_data );

        void saveScaling( const std::string &scale_file );
        void loadScaling( const std::string &scale_file );

        std::vector<FeatureScaler> getScalers() const { return scalers_; }

        void setScalers(const std::vector<FeatureScaler> & scalers)
        {
            scalers_ = scalers;
        }
    private:
        std::vector<FeatureScaler> scalers_;
};


/**
 * @brief wrap of SVM implementation from LibSVM with scaling descriptors
 *
 * @tparam F enum representing composite, to be used for extraction 
 *
 * This class integrates LibSVM implementation of SVM with C++. These SVM 
 * supports probability outputs.
 */
template <feature F>
class ScalingLibSVM
{
    public:
        ScalingLibSVM() 
        {
            svm_ = nullptr;
        }

        ~ScalingLibSVM() 
        {
            if ( svm_ != nullptr )
            {
                if (svm_->free_sv != 1)
                {
                    svm_free_and_destroy_model(&svm_);
                }
                else
                {
                    bridge_.destroy_svm_model(&svm_);
                }
                svm_ = nullptr;
            }
        }


        /**
         * @brief returns number of classes
         *
         * @return number of classes
         */
        int getNumberOfClasses() const 
        {
            return number_of_classes_; 
        }

        /**
         * @brief train data without any scaling
         *
         * @param data_file path to file
         * @param param parameter for svm training
         *
         * See the libsvm documentation for more detail on
         * parameters for training svm.
         */
        void train( const std::string &data_file, svm_parameter *param )
        {
            cv::Mat train_data, labels;
            LoadTrainData<F>::load( data_file, train_data, labels );
            data_scaling_.setUp( train_data );
            svm_ = bridge_.train( train_data, labels, param );
            number_of_classes_ = svm_get_nr_class( svm_ );
        }

        /**
         * @brief loads configuration from \p conf_file
         *
         * @param conf_file path to the configuration
         */
        void saveConfiguration( const std::string &conf_file)
        {
            bridge_.save(conf_file, svm_, data_scaling_.getScalers());
            // int result = svm_save_model( conf_file.c_str(), svm_ );
            // if (result != 0)
            // {
            //     throw ActionError( "saving to " + conf_file );
            // }
        }

        /**
         * @brief loads configuration from \p conf_file
         *
         * @param conf_file path to the configuration
         * @throw FileNotFoundException when file \p conf_file doesn't exist
         */
        void loadConfiguration( const std::string &conf_file)
        {
            // svm_ = svm_load_model( conf_file.c_str() );
            // //TODO
            // if ( svm_ == nullptr )
            // {
            //     throw FileNotFoundException(conf_file + ", libsvm configuration not found");
            // }
            //
            auto scalers = data_scaling_.getScalers();
            svm_ = bridge_.load(conf_file, scalers);
            data_scaling_.setScalers(scalers);
            number_of_classes_ = svm_get_nr_class( svm_ );
            // data_scaling_.loadScaling(scale_file);
        }


        /**
         * @brief predicts class for feature vector sample
         *
         * @param sample vector of features
         *
         * @return label of predicted class
         */
        float predict(const std::vector<float> &data ) 
        {
            NOCR_ASSERT( svm_ != nullptr , "no configuration loaded yet" );

            auto scaled_desc = data_scaling_.scale(data);

            svm_node *nodes = bridge_.constructSample( 
                                    data_scaling_.scale(data) );
            float out = svm_predict( svm_ , nodes );
            delete[] nodes;
            return out;
        }

        /**
         * @brief predicts class for feature vector sample
         *
         * @param sample vector of features
         * @param probabilities probability outputs from classification 
         * will be stored in this vector
         *
         * @return label of predicted class
         *
         * If SVM isn't trained for probability outputs, exception will be thrown.
         */
        double predictProbabilities(const std::vector<float> &data, 
                                    std::vector<double> &probabilities ) 
        {
            NOCR_ASSERT( svm_ != nullptr , "no configuration loaded yet" );


            bridge_.constructSample( data_scaling_.scale(data), &nodes_[0]);

            probabilities.resize( number_of_classes_ );
            double out = svm_predict_probability( svm_ , &nodes_[0], 
                                                probabilities.data() ); 
            return out;
        }

    private:
        LibSVMTrainBridge bridge_;
        svm_model *svm_;
        int number_of_classes_;
        DataScaling data_scaling_;
        std::array<svm_node, FeatureTraits<F>::features_length + 1> nodes_;
};

#endif /* classifier_wrap.h */
